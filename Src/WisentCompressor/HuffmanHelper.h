#ifndef HUFFMAN_ENCODER_H
#define HUFFMAN_ENCODER_H

#ifdef __cplusplus
#include <string>
#include <unordered_map>
extern "C" {
#endif

#include <stdint.h> 

typedef struct 
{
    uint8_t *encoded_data; 
    uint32_t encoded_size; 
} HuffmanEncodedData;

void huffman_init();

HuffmanEncodedData huffman_encode(const uint8_t *data, uint32_t data_size);

uint8_t *huffman_decode(const HuffmanEncodedData *encoded_data, uint32_t *decoded_size);

void huffman_free_encoded_data(HuffmanEncodedData *encoded_data);

void huffman_free_decoded_data(uint8_t *decoded_data);

#ifdef __cplusplus
}

namespace huffman {

    struct Node {
        char symbol;
        int frequency;
        Node *left;
        Node *right;

        Node(char symbol, int frequency) : symbol(symbol), frequency(frequency), left(nullptr), right(nullptr) {}
    };

    class Encoder {
    public:
        Encoder();
        ~Encoder();

        std::string encode(const std::string &input);

        std::string decode(const std::string &encoded);

        Node *getTree() const;

        void printHuffmanCodes() const;

    private:
        Node *root;  
        std::unordered_map<char, std::string> codes; 
        std::unordered_map<std::string, char> reverse_codes; 

        void buildTree(const std::string &input);

        void generateCodes(Node *node, const std::string &code);

        void deleteTree(Node *node);
    };
}

#endif // __cplusplus

#endif // HUFFMAN_ENCODER_H
