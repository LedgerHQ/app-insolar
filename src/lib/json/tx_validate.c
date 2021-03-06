/*******************************************************************************
*   (c) 2018, 2019 ZondaX GmbH
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************/

#include <jsmn.h>
#include <zxmacros.h>
#include <parser_common.h>
#include "json/json_parser.h"

#define EQUALS(_P, _Q, _LEN) (MEMCMP( (_P), (_Q), (_LEN))==0)

const char whitespaces[] = {
        0x20,// space ' '
        0x0c, // form_feed '\f'
        0x0a, // line_feed, '\n'
        0x0d, // carriage_return, '\r'
        0x09, // horizontal_tab, '\t'
        0x0b // vertical_tab, '\v'
};

int8_t is_space(char c) {
    for (uint16_t i = 0; i < sizeof(whitespaces); i++) {
        if (whitespaces[i] == c) {
            return 1;
        }
    }
    return 0;
}

int8_t contains_whitespace(parsed_json_t *json) {
    int start = 0;
    const int last_element_index = json->tokens[0].end;

    // Starting at token 1 because token 0 contains full tx
    for (int i = 1; i < json->numberOfTokens; i++) {
        if (json->tokens[i].type != JSMN_UNDEFINED) {
            const int end = json->tokens[i].start;
            for (int j = start; j < end; j++) {
                if (is_space(json->buffer[j]) == 1) {
                    return 1;
                }
            }
            start = json->tokens[i].end + 1;
        } else {
            return 0;
        }
    }
    while (start <= last_element_index && json->buffer[start] != '\0') {
        if (is_space(json->buffer[start])) {
            return 1;
        }
        start++;
    }
    return 0;
}

int8_t is_sorted(int16_t first_index,
    int16_t second_index,
                 parsed_json_t *json) {
#if DEBUG_SORTING
    char first[256];
    char second[256];

    int size =  parsed_tx->Tokens[first_index].end - parsed_tx->Tokens[first_index].start;
    strncpy(first, tx + parsed_tx->Tokens[first_index].start, size);
    first[size] = '\0';
    size =  parsed_tx->Tokens[second_index].end - parsed_tx->Tokens[second_index].start;
    strncpy(second, tx + parsed_tx->Tokens[second_index].start, size);
    second[size] = '\0';
#endif

    if (strcmp((const char *) (json->buffer+json->tokens[first_index].start),
               (const char *) (json->buffer+json->tokens[second_index].start)) <= 0) {
        return 1;
    }
    return 0;
}

int8_t dictionaries_sorted(parsed_json_t *json) {
    for (int i = 0; i < json->numberOfTokens; i++) {
        if (json->tokens[i].type == JSMN_OBJECT) {

            const int count = object_get_element_count(i, json);
            if (count > 1) {
                int prev_token_index = object_get_nth_key(i, 0, json);
                for (int j = 1; j < count; j++) {
                    int next_token_index = object_get_nth_key(i, j, json);
                    if (!is_sorted(prev_token_index, next_token_index, json)) {
                        return 0;
                    }
                    prev_token_index = next_token_index;
                }
            }
        }
    }
    return 1;
}

parser_error_t tx_validate(parsed_json_t *json) {
    if (contains_whitespace(json) == 1) {
        return parser_json_contains_whitespace;
    }

//    if (dictionaries_sorted(json) != 1) {
//        return parser_json_is_not_sorted;
//    }


//     auto transaction = R"({"jsonrpc":"2.0","id":1,
//     "method":"contract.call","params":
//     {"callSite":"member.transfer","callParams":{"amount":"20000000000","toMemberReference":"insolar:1AAEAATjWvxVC3DFEBqINu7JSKWxlcb_uJo7QdAHrcP8"},
//     "reference":"insolar:1AAEAAY2zkW3pOTCIlYg6XqhWLR32AAeBpTKZ3vLdFhE","publicKey":"-----BEGIN PUBLIC KEY-----\nMFYwEAYHKoZIzj0CAQYFK4EEAAoDQgAEc+Vs4y+XWE77LR0QL1e1wCOFePFEHJIB\ndsPWPKMH5zGRhRWV1HCJXajENCV2bdG/YKKEOAzTdE5BGXNg2dRQpQ==\n-----END PUBLIC KEY-----",
//     "seed":"rOpiqgDHHDmr2PfI5UzZiWpjRyDMoFtBNFoOwyC+yJ8="}})";

    // find method
    const int16_t method_token_idx = object_get_value(json, ROOT_TOKEN_INDEX, token_key_method);
    if (method_token_idx == -1) {
        return parser_json_missing_method;
    }

    // method should be contract.call
    if (!EQUALS("contract.call",
               json->buffer + json->tokens[method_token_idx].start,
               json->tokens[method_token_idx].end - json->tokens[method_token_idx].start)) {
        return parser_json_unsupported_method;
    }

    // find params
    const int16_t  params_token_idx = object_get_value(json, ROOT_TOKEN_INDEX, token_key_params);
    // params should be object
    if (json->tokens[method_token_idx].type == JSMN_OBJECT) {
        return parser_json_unexpected_params;
    }

    // params object should contain keys: [seed, publicKey, callSite] and optional key callParams
    int16_t idx = object_get_value(json, params_token_idx,token_key_callsite);
    if (idx == -1) {
        return parser_json_missing_callsite;
    }

    idx = object_get_value(json, params_token_idx, token_key_seed);
    if (idx == -1) {
        return parser_json_missing_seed;
    }

    idx = object_get_value(json, params_token_idx, token_key_public_key);
    if (idx == -1) {
        return parser_json_missing_public_key;
    }

    return parser_ok;
}
