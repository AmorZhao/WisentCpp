// Simple LZW compressing and decompressing algorithm
// To run this script directly: dotnet script LZW.cs
// To install dotnet-script: dotnet tool install --global dotnet-script
//      export PATH="$PATH:$HOME/.dotnet/tools"

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;

class LZW
{
    public static List<int> Compress(string input)
    {
        var dictionary = new Dictionary<string, int>();

        for (int i = 0; i < 256; i++)
        {
            dictionary.Add(((char)i).ToString(), i);
        }
        var current = string.Empty;
        var compressed = new List<int>();
        foreach (var c in input)
        {
            var combined = current + c;
            if (dictionary.ContainsKey(combined))
            {
                current = combined;
                continue; 
            }
            compressed.Add(dictionary[current]);
            dictionary.Add(combined, dictionary.Count);
            current = c.ToString();
        }
        if (!string.IsNullOrEmpty(current))
        {
            compressed.Add(dictionary[current]);
        } 
        return compressed;
    }

    public static string Decompress(List<int> compressed)
    {
        var dictionary = new Dictionary<int, string>();
        for (int i = 0; i < 256; i++)
        {
            dictionary.Add(i, ((char)i).ToString());
        }
        var previous = dictionary[compressed[0]];
        var output = previous;
        for (int i = 1; i < compressed.Count; i++)
        {
            string entry = dictionary.ContainsKey(compressed[i]) 
                        ? dictionary[compressed[i]] 
                        : previous + previous[0];
            output += entry;
            dictionary.Add(dictionary.Count, previous + entry[0]);
            previous = entry;
        }
        return output;
    }
}

string filePath = "test/"; 
string inputFile = "original.txt";
string compressedFile = "compressed.txt";
string decompressedFile = "decompressed.txt";

string inputText = File.ReadAllText(filePath + inputFile);

List<int> compressedData = LZW.Compress(inputText);
File.WriteAllBytes(filePath+compressedFile, compressedData.SelectMany(BitConverter.GetBytes).ToArray());

string decompressedText = LZW.Decompress(compressedData);
File.WriteAllText(filePath+decompressedFile, decompressedText);

Console.WriteLine($"Decompression successful: {inputText == decompressedText}"); 

Console.WriteLine($"Original text: {inputText.Length / 1000} KB");
Console.WriteLine($"Compressed text: {compressedData.Count * sizeof(int) / 1000} KB");