using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

class LZ77
{
    public static List<Tuple<int, int, char>> Compress(string input, int windowSize = 8192, int lookaheadBufferSize = 64)
    {
        var compressed = new List<Tuple<int, int, char>>();
        int inputLength = input.Length;
        int codingPosition = 0;

        while (codingPosition < inputLength)
        {
            int matchedLength = 0;
            int offset = 0;
            char nextChar = input[codingPosition];

            int searchStart = Math.Max(0, codingPosition - windowSize);
            int searchEnd = codingPosition;

            for (int i = searchStart; i < searchEnd; i++)
            {
                int length = 0;
                while (length < lookaheadBufferSize 
                    && codingPosition + length < inputLength 
                    && input[i + length] == input[codingPosition + length])
                {
                    length++;
                }

                if (length > matchedLength)
                {
                    matchedLength = length;
                    offset = codingPosition - i;
                    if (codingPosition + matchedLength < inputLength)
                    {
                        nextChar = input[codingPosition + matchedLength];
                    }
                }
            }

            compressed.Add(new Tuple<int, int, char>(offset, matchedLength, nextChar));
            codingPosition += matchedLength + 1;
        }

        return compressed;
    }

    public static string Decompress(List<Tuple<int, int, char>> compressed)
    {
        var decompressed = new List<char>();

        foreach (var tuple in compressed)
        {
            int offset = tuple.Item1;
            int length = tuple.Item2;
            char nextChar = tuple.Item3;

            int start = decompressed.Count - offset;
            for (int i = 0; i < length; i++)
            {
                decompressed.Add(decompressed[start + i]);
            }

            decompressed.Add(nextChar);
        }

        return new string(decompressed.ToArray());
    }
}

string filePath = "test/";
string inputFile = "original.txt";
string compressedFile = "compressed.txt";
string decompressedFile = "decompressed.txt";

string inputText = File.ReadAllText(filePath + inputFile);

List<Tuple<int, int, char>> compressedData = LZ77.Compress(inputText);
File.WriteAllBytes(filePath + compressedFile, compressedData.SelectMany(t => BitConverter.GetBytes(t.Item1).Concat(BitConverter.GetBytes(t.Item2)).Concat(new byte[] { (byte)t.Item3 })).ToArray());

string decompressedText = LZ77.Decompress(compressedData);
File.WriteAllText(filePath + decompressedFile, decompressedText);

Console.WriteLine($"Decompression successful: {inputText == decompressedText}");

Console.WriteLine($"Original text: {inputText.Length / 1000} KB");
Console.WriteLine($"Compressed text: {compressedData.Count * (sizeof(int) * 2 + sizeof(char)) / 1000} KB");