using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Collections;

class HuffmanNode
{
    public char Symbol { get; set; }
    public int Frequency { get; set; }
    public HuffmanNode Left { get; set; }
    public HuffmanNode Right { get; set; }

    public List<bool> Traverse(char symbol, List<bool> data)
    {
        if (Right == null && Left == null)
        {
            return symbol.Equals(Symbol) ? data : null;
        }
        else
        {
            List<bool> left = null;
            List<bool> right = null;

            if (Left != null)
            {
                var leftPath = new List<bool>();
                leftPath.AddRange(data);
                leftPath.Add(false);

                left = Left.Traverse(symbol, leftPath);
            }

            if (Right != null)
            {
                var rightPath = new List<bool>();
                rightPath.AddRange(data);
                rightPath.Add(true);

                right = Right.Traverse(symbol, rightPath);
            }

            return left ?? right;
        }
    }
}

class HuffmanTree
{
    private List<HuffmanNode> nodes = new List<HuffmanNode>();
    public HuffmanNode Root { get; set; }
    public Dictionary<char, int> Frequencies = new Dictionary<char, int>();

    public void Build(string source)
    {
        for (int i = 0; i < source.Length; i++)
        {
            if (!Frequencies.ContainsKey(source[i]))
            {
                Frequencies.Add(source[i], 0);
            }

            Frequencies[source[i]]++;
        }

        foreach (KeyValuePair<char, int> symbol in Frequencies)
        {
            nodes.Add(new HuffmanNode() { Symbol = symbol.Key, Frequency = symbol.Value });
        }

        while (nodes.Count > 1)
        {
            List<HuffmanNode> orderedNodes = nodes.OrderBy(node => node.Frequency).ToList<HuffmanNode>();

            if (orderedNodes.Count >= 2)
            {
                List<HuffmanNode> taken = orderedNodes.Take(2).ToList<HuffmanNode>();

                HuffmanNode parent = new HuffmanNode()
                {
                    Symbol = '*',
                    Frequency = taken[0].Frequency + taken[1].Frequency,
                    Left = taken[0],
                    Right = taken[1]
                };

                nodes.Remove(taken[0]);
                nodes.Remove(taken[1]);
                nodes.Add(parent);
            }

            this.Root = nodes.FirstOrDefault();
        }
    }

    public BitArray Encode(string source)
    {
        List<bool> encodedSource = new List<bool>();

        for (int i = 0; i < source.Length; i++)
        {
            List<bool> encodedSymbol = this.Root.Traverse(source[i], new List<bool>());
            encodedSource.AddRange(encodedSymbol);
        }

        BitArray bits = new BitArray(encodedSource.ToArray());

        return bits;
    }

    public string Decode(BitArray bits)
    {
        HuffmanNode current = this.Root;
        string decoded = "";

        foreach (bool bit in bits)
        {
            if (bit)
            {
                if (current.Right != null)
                {
                    current = current.Right;
                }
            }
            else
            {
                if (current.Left != null)
                {
                    current = current.Left;
                }
            }

            if (IsLeaf(current))
            {
                decoded += current.Symbol;
                current = this.Root;
            }
        }

        return decoded;
    }

    public bool IsLeaf(HuffmanNode node)
    {
        return (node.Left == null && node.Right == null);
    }
}

string filePath = "test/";
string inputFile = "original.txt";
string compressedFile = "compressed.bin";
string decompressedFile = "decompressed.txt";

string inputText = File.ReadAllText(filePath + inputFile);

HuffmanTree huffmanTree = new HuffmanTree();
huffmanTree.Build(inputText);

BitArray encoded = huffmanTree.Encode(inputText);
byte[] encodedBytes = new byte[(encoded.Length - 1) / 8 + 1];
encoded.CopyTo(encodedBytes, 0);
File.WriteAllBytes(filePath + compressedFile, encodedBytes);

BitArray encodedBits = new BitArray(File.ReadAllBytes(filePath + compressedFile));
string decoded = huffmanTree.Decode(encodedBits);
File.WriteAllText(filePath + decompressedFile, decoded);

Console.WriteLine($"Decompression successful: {inputText == decoded}");

Console.WriteLine($"Original text: {inputText.Length / 1000} KB");
Console.WriteLine($"Compressed text: {encodedBytes.Length / 1000} KB");

