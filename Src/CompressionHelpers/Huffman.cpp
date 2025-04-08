#include <vector>
#include <queue>
#include <unordered_map>
#include <bitset>
#include "Huffman.hpp"

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

class HuffmanTree 
{
  private:
    HuffmanNode* root;
    std::unordered_map<char, std::string> encodingTable;

    void buildEncodingTable(HuffmanNode* node, const std::string& str) 
    {
        if (!node) return;
        if (!node->left && !node->right) 
        {
            encodingTable[node->symbol] = str;
        }
        buildEncodingTable(node->left, str + "0");
        buildEncodingTable(node->right, str + "1");
    }

  public:
    HuffmanTree() : root(nullptr) {}

    void buildTreeWithInput(const std::vector<uint8_t>& input) 
    {
        std::unordered_map<char, int64_t> frequencies;
        for (char c : input) 
        {
            frequencies[c]++;
        }
        frequencies['\0'] = 1; // EOF symbol

        std::priority_queue<HuffmanNode*, std::vector<HuffmanNode*>, Compare> tree;
        for (auto& pair : frequencies) 
        {
            tree.push(new HuffmanNode(pair.first, pair.second));
        }

        while (tree.size() > 1) 
        {
            HuffmanNode* left = tree.top(); 
            tree.pop();
            
            HuffmanNode* right = tree.top(); 
            tree.pop();

            HuffmanNode* parent = new HuffmanNode(
                '*', 
                left->frequency + right->frequency
            );
            parent->left = left;
            parent->right = right;

            tree.push(parent);
        }

        root = tree.top();
        buildEncodingTable(root, "");
    }

    void buildTreeWithEncodingTable(
        const std::unordered_map<char, std::string>& encodingTable
    ) {
        root = new HuffmanNode('*', 0);
        size_t index = 0;

        for (const auto& pair : encodingTable) 
        {
            HuffmanNode* current = root;
            for (char bit : pair.second) 
            {
                if (bit == '0') 
                {
                    if (!current->left) 
                    {
                        current->left = new HuffmanNode('*', 0);
                    }
                    current = current->left;
                } 
                else 
                {
                    if (!current->right) 
                    {
                        current->right = new HuffmanNode('*', 0);
                    }
                    current = current->right;
                }
            }
            current->symbol = pair.first; 
        }
    }

    std::vector<uint8_t> encode(const std::vector<uint8_t>& data) 
    {
        std::vector<uint8_t> encodedBytes;
	    encodedBytes.reserve(data.size());

        // Encode EOF symbol
        std::string eofCode = encodingTable['\0'];
        encodedBytes.push_back(eofCode.size());
        encodeStringToBytes(eofCode, encodedBytes);

        // Add encoding table
        for (const auto& pair : encodingTable) 
        {
            if (pair.first == '\0') continue;

            encodedBytes.push_back(pair.first);

            uint8_t codeLength = pair.second.size();
            encodedBytes.push_back(codeLength);

            encodeStringToBytes(pair.second, encodedBytes);

            encodedBytes.push_back(0); // Delimiter
        }
        encodedBytes.push_back(0); // Final delimiter

        // Encode data 
        std::vector<bool> encoded;
	    encoded.reserve(data.size());
        for (char c : data) 
        {
            for (char bit : encodingTable[c]) 
            {
                encoded.push_back(bit == '1' ? 1 : 0);
            }
        }
        for (char bit : encodingTable['\0'])  // EOF symbol
        {
            encoded.push_back(bit == '1' ? 1 : 0);
        }

        // Convert to bytes
        uint8_t byte = 0;
        uint8_t bitCount = 0;

        for (bool bit : encoded) 
        {
            byte = (byte << 1) | bit;
            bitCount++;
            if (bitCount == 8) 
            {
                encodedBytes.push_back(byte);
                byte = 0;
                bitCount = 0;
            }
        }
        if (bitCount > 0) 
        {
            byte <<= (8 - bitCount);
            encodedBytes.push_back(byte);
        }

        return encodedBytes;
    }

    std::vector<uint8_t> decode(const std::vector<uint8_t>& data) 
    {
        std::vector<uint8_t> decoded;
	    decoded.reserve(data.size());
        HuffmanNode* current = root;

        std::vector<bool> bits;
	    bits.reserve(data.size());
        for (size_t i = 0; i < data.size(); ++i) 
        {
            std::bitset<8> byte(data[i]);
            for (int j = 7; j >= 0; --j) 
            {
                bits.push_back(byte[j]);
            }
        }

        for (bool bit : bits) 
        {
            current = bit ? current->right : current->left;
            if (current->left == nullptr && current->right == nullptr) 
            {
                if (current->symbol == '\0') break;  // EOF 
                decoded.push_back((uint8_t)current->symbol);
                current = root;
            }
        }
        return decoded;
    }

    void encodeStringToBytes(
        const std::string& str, 
        std::vector<uint8_t>& encodedBytes
    ) {
        uint8_t bitBuffer = 0;
        int64_t bitCount = 0;
        for (char bit : str) 
        {
            bitBuffer = (bitBuffer << 1) | (bit == '1' ? 1 : 0);
            bitCount++;
            if (bitCount % 8 == 0) 
            {
                encodedBytes.push_back(bitBuffer);
                bitBuffer = 0;
                bitCount = 0; 
            }
        }
        if (bitCount > 0) 
        {
            bitBuffer <<= (8 - bitCount);
            encodedBytes.push_back(bitBuffer);
        }
    }

    std::unordered_map<char, std::string> getEncodingTable() 
    {
        return encodingTable;
    }
}; 

namespace Huffman
{
    std::vector<uint8_t> HuffmanCoder::compress(
        const std::vector<uint8_t>& input
    ) {    
        HuffmanTree huffmanTree;
        huffmanTree.buildTreeWithInput(input);

        std::vector<uint8_t> encoded = huffmanTree.encode(input);
        
        return encoded;
    } 

    std::string byteToBinaryString(uint8_t byte, uint8_t stringLength) 
    {
        std::bitset<8> eofBits(byte);
        std::string binaryString = eofBits.to_string();
        return (stringLength >= 8) ? binaryString : binaryString.substr(0, stringLength);
    }

    std::vector<uint8_t> HuffmanCoder::decompress(
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
    }
} // Huffman
