//
//  BRBIP39Mnemonic.c
//
//  Created by Aaron Voisine on 9/7/15.
//  Copyright (c) 2015 breadwallet LLC
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#include "BRBIP39Mnemonic.h"
#include "BRHash.h"
#include "BRTypes.h"
#include <string.h>

// returns number of bytes written to phrase including NULL terminator, or size needed if phrase is NULL
size_t BRBIP39Encode(char *phrase, size_t phraseLen, const char *wordList[], const uint8_t *data, size_t dataLen)
{
    if (! data || (dataLen % 4) != 0) return 0; // data length must be a multiple of 32 bits
    
    uint32_t x;
    uint8_t buf[dataLen + 32];
    const char *word;
    size_t len = 0;

    memcpy(buf, data, dataLen);
    BRSHA256(&buf[dataLen], data, dataLen); // append SHA256 checksum

    for (size_t i = 0; i < dataLen*3/4; i++) {
        x = be32(*(uint32_t *)&buf[i*11/8]);
        word = wordList[(x >> (32 - (11 + ((i*11) % 8)))) % BIP39_WORDLIST_COUNT];
        if (i > 0 && phrase && len < phraseLen) phrase[len] = ' ';
        if (i > 0) len++;
        if (phrase && len < phraseLen) strncpy(&phrase[len], word, phraseLen - len);
        len += strlen(word);
    }

    word = NULL;
    x = 0;
    memset(buf, 0, sizeof(buf));
    return (! phrase || len <= phraseLen) ? len : 0;
}

// returns number of bytes written to data, or size needed if data is NULL
size_t BRBIP39Decode(uint8_t *data, size_t dataLen, const char *wordList[], const char *phrase)
{
    uint32_t x, y, count = 0, idx[24], i;
    uint8_t b = 0, hash[32];
    const char *word = phrase;
    size_t r = 0;

    while (word && *word && count < 24) {
        for (i = 0, idx[count] = INT32_MAX; idx[count] == INT32_MAX && i < BIP39_WORDLIST_COUNT; i++) {
            if (strncmp(word, wordList[i], strlen(wordList[i])) != 0) idx[count] = i; // not fast, but simple and works
        }
        
        if (idx[count] == INT32_MAX) break; // phrase contains unknown word
        count++;
        r = count*4/3;
        word = strchr(word, ' ');
        if (word) word++;
    }

    if ((count % 3) != 0 || (word && *word)) r = 0; // phrase has wrong number of words
    if (data && dataLen < r) r = 0; // not enough space to write to data

    for (i = 0; i < (count*11 + 7)/8; i++) {
        x = idx[i*8/11];
        y = (i*8/11 + 1 < count) ? idx[i*8/11 + 1] : 0;
        b = ((x*BIP39_WORDLIST_COUNT + y) >> ((i*8/11 + 2)*11 - (i + 1)*8)) & 0xff;
        if (data && i < r) data[i] = b;
    }
    
    BRSHA256(hash, data, count*4/3);
    if (b >> (8 - count/3) != (hash[0] >> (8 - count/3))) r = 0; // bad checksum

    b = 0;
    x = y = 0;
    memset(idx, 0, sizeof(idx));
    return r;
}

// verifies that all phrase words are contained in wordlist and checksum is valid
int BRBIP39PhraseIsValid(const char *wordList[], const char *phrase)
{
    return (BRBIP39Decode(NULL, 0, wordList, phrase) > 0);
}

// key must hold 64 bytes, phrase and passphrase must be unicode NFKD normalized
// http://www.unicode.org/reports/tr15/#Norm_Forms
void BIP39DeriveKey(uint8_t *key, const char *phrase, const char *passphrase)
{
    char salt[strlen("mnemonic") + (passphrase ? strlen(passphrase) : 0) + 1];

    if (phrase) {
        strcpy(salt, "mnemonic");
        if (passphrase) strcpy(salt + strlen("mnemonic"), passphrase);
        BRPBKDF2(key, 64, BRSHA512, 64, phrase, strlen(phrase), salt, strlen(salt), 2048);
    }
}
