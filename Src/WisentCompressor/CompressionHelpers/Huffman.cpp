#include "Huffman.hpp"
#include <queue>
#include <unordered_map>
#include <bitset>
#include <cstring>
            
struct HuffmanNode 
{
    char symbol;
    int64_t frequency;
    HuffmanNode* left;
    HuffmanNode* right;
    HuffmanNode(char s, int64_t f) : symbol(s), frequency(f), left(nullptr), right(nullptr) {}
};

struct Compare 
{
    bool operator()(HuffmanNode* l, HuffmanNode* r) 
    {
        return l->frequency > r->frequency;
    }
};

class HuffmanTree {
private:
    HuffmanNode* root = nullptr;
    std::unordered_map<char, std::string> encodingTable;

    void buildEncodingTable(HuffmanNode* node, const std::string& str) {
        if (!node) return;
        if (!node->left && !node->right) {
            encodingTable[node->symbol] = str;
        }
        buildEncodingTable(node->left, str + "0");
        buildEncodingTable(node->right, str + "1");
    }

public:
    void buildTreeWithInput(const std::byte* input, size_t inputSize) {
        std::unordered_map<char, int64_t> frequencies;
        for (size_t i = 0; i < inputSize; ++i) {
            frequencies[static_cast<char>(input[i])]++;
        }
        frequencies['\0'] = 1; // EOF

        std::priority_queue<HuffmanNode*, std::vector<HuffmanNode*>, Compare> tree;
        for (auto& [ch, freq] : frequencies) {
            tree.push(new HuffmanNode(ch, freq));
        }

        while (tree.size() > 1) {
            HuffmanNode* left = tree.top(); tree.pop();
            HuffmanNode* right = tree.top(); tree.pop();

            HuffmanNode* parent = new HuffmanNode('*', left->frequency + right->frequency);
            parent->left = left;
            parent->right = right;
            tree.push(parent);
        }

        root = tree.top();
        buildEncodingTable(root, "");
    }

    std::vector<uint8_t> encode(const std::byte* input, size_t inputSize) {
        std::vector<uint8_t> encodedBytes;

        std::string eofCode = encodingTable['\0'];
        encodedBytes.push_back(static_cast<uint8_t>(eofCode.size()));
        encodeStringToBytes(eofCode, encodedBytes);

        for (const auto& [symbol, code] : encodingTable) {
            if (symbol == '\0') continue;
            encodedBytes.push_back(static_cast<uint8_t>(symbol));
            encodedBytes.push_back(static_cast<uint8_t>(code.size()));
            encodeStringToBytes(code, encodedBytes);
            encodedBytes.push_back(0); // Delimiter
        }
        encodedBytes.push_back(0); // Final delimiter

        // Encode actual data
        std::vector<bool> bits;
        for (size_t i = 0; i < inputSize; ++i) {
            for (char bit : encodingTable[static_cast<char>(input[i])]) {
                bits.push_back(bit == '1');
            }
        }
        for (char bit : encodingTable['\0']) {
            bits.push_back(bit == '1');
        }

        uint8_t byte = 0, count = 0;
        for (bool bit : bits) {
            byte = (byte << 1) | bit;
            ++count;
            if (count == 8) {
                encodedBytes.push_back(byte);
                byte = 0;
                count = 0;
            }
        }
        if (count > 0) {
            byte <<= (8 - count);
            encodedBytes.push_back(byte);
        }

        return encodedBytes;
    }

    void encodeStringToBytes(const std::string& str, std::vector<uint8_t>& out) {
        uint8_t buffer = 0;
        int count = 0;
        for (char bit : str) {
            buffer = (buffer << 1) | (bit == '1' ? 1 : 0);
            ++count;
            if (count == 8) {
                out.push_back(buffer);
                buffer = 0;
                count = 0;
            }
        }
        if (count > 0) {
            buffer <<= (8 - count);
            out.push_back(buffer);
        }
    }
};

static Result<size_t> wisent::algorithms::Huffman::compress(
    const std::byte* input,
    const size_t inputSize,
    const std::byte* output
) {
    if (!input || !output || inputSize == 0) 
        return makeError<size_t>("Invalid input or output buffer");

    HuffmanTree tree;
    tree.buildTreeWithInput(input, inputSize);
    std::vector<uint8_t> encoded = tree.encode(input, inputSize);

    std::memcpy(output, encoded.data(), encoded.size());
    return makeResult<size_t>(encoded.size());
}; 


static std::string byteToBinaryString(uint8_t byte, uint8_t stringLength) 
{
    std::bitset<8> eofBits(byte);
    std::string binaryString = eofBits.to_string();
    return (stringLength >= 8) ? binaryString : binaryString.substr(0, stringLength);
}; 

static std::vector<uint8_t> wisent::algorithms::Huffman::decompress(
    const std::vector<uint8_t>& input
) {
    HuffmanTree huffmanTree;
    std::unordered_map<char, std::string> encodingTable;

    uint8_t offset = 0; 

    // Read EOF symbol
    uint8_t eofCodeLength = input[offset++];
    int maxCodeLengthInByte = (eofCodeLength + 7 ) / 8;

    std::string eofBitString;
    for (int i = 1; i <= maxCodeLengthInByte; i++) 
    {
        eofBitString += byteToBinaryString(input[offset++], 8);
    }
    eofBitString = eofBitString.substr(0, eofCodeLength);        
    encodingTable['\0'] = eofBitString;

    // Read encoding table
    while (offset < input.size()) 
    {
        uint8_t symbol = input[offset++];
        if (symbol == 0) break; // End of encoding table

        uint8_t codeLength = input[offset++]; 
        int codeLengthInByte = (codeLength + 7) / 8;
        
        std::string bitString;
        uint8_t byteCount = 0; 
        while (offset < input.size() && byteCount < codeLengthInByte) 
        {
            bitString += byteToBinaryString(input[offset++], 8);
            byteCount++;
        }
        bitString = bitString.substr(0, codeLength);  
        encodingTable[symbol] = bitString;
        offset ++; // Skip delimiter
    }
    huffmanTree.buildTreeWithEncodingTable(encodingTable);

    std::vector<uint8_t> data(input.begin() + offset, input.end());

    std::vector<uint8_t> decoded = huffmanTree.decode(data);

    return decoded; 
}; 
