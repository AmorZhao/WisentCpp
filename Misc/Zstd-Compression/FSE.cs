using System;
using System.Collections.Generic;
using System.Linq;

namespace FiniteStateEntropy
{
    public static class FSE
    {
        private const int DefaultMaxSymbolValue = 255;
        private const int DefaultTableLog = 11;
        private const int MinTableLog = 5;
        private const int MaxTableLog = 15;

        // Compression Structures
        private struct SymbolCompressionTransform
        {
            public uint StateBasedBitsOut;
            public uint MaxBitsOut;
            public uint MinStatePlus;
            public uint NextStateOffset;
        }

        private class CTable
        {
            public int TableLog { get; set; }
            public int MaxSymbolValue { get; set; }
            public List<ushort> StateTable { get; set; } = new();
            public List<SymbolCompressionTransform> SymbolTransformTable { get; set; } = new();
        }

        private struct SymbolDecompressionTransform
        {
            public ushort NewState;
            public byte Symbol;
            public byte NumberOfBitsToRead;
        }

        private static int GetHighestBitPosition(uint value) => 31 - BitOperations.LeadingZeroCount(value);

        private static int GetOptimalTableLog(int maxTableLog, int inputSize, int maxSymbolValue)
        {
            int maxBitsSrc = GetHighestBitPosition((uint)(inputSize - 1));
            int tableLog = maxTableLog;

            if (inputSize <= 1)
                Console.WriteLine("Warning: Source size too small, RLE should be used instead");

            if (tableLog == 0) tableLog = DefaultTableLog;
            if (maxBitsSrc < tableLog) tableLog = maxBitsSrc;
            if (tableLog < MinTableLog) tableLog = MinTableLog;
            if (tableLog > maxTableLog) tableLog = maxTableLog;

            return tableLog;
        }

        private static List<int> CountSymbols(ref int maxSymbolValue, List<byte> input)
        {
            var count = new List<int>(new int[maxSymbolValue + 1]);

            foreach (var symbol in input)
            {
                if (symbol > maxSymbolValue)
                    throw new InvalidOperationException("Symbol value exceeds maxSymbolValue");

                count[symbol]++;
            }

            while (maxSymbolValue > 0 && count[maxSymbolValue] == 0)
                maxSymbolValue--;

            return count;
        }

        private static List<short> NormalizeCount(int tableLog, List<int> symbolCounts, int totalSymbols, ref int maxSymbolValue)
        {
            var normalizedCounts = new List<short>(new short[maxSymbolValue + 1]);
            long scale = 62 - tableLog;
            long step = (1L << 62) / totalSymbols;
            int remainingNormalizedSymbols = 1 << tableLog;
            int lowThreshold = totalSymbols >> tableLog;

            for (int symbol = 0; symbol <= maxSymbolValue; symbol++)
            {
                if (symbolCounts[symbol] == 0)
                {
                    normalizedCounts[symbol] = 0;
                    continue;
                }

                if (symbolCounts[symbol] <= lowThreshold)
                {
                    normalizedCounts[symbol] = -1;
                    remainingNormalizedSymbols--;
                }
                else
                {
                    short probability = (short)((symbolCounts[symbol] * step) >> scale);
                    normalizedCounts[symbol] = probability;
                    remainingNormalizedSymbols -= probability;
                }
            }

            return normalizedCounts;
        }

        private static CTable BuildCTable(List<short> normalizedCounts, int maxSymbolValue, int tableLog)
        {
            var cTable = new CTable
            {
                TableLog = tableLog,
                MaxSymbolValue = maxSymbolValue
            };

            int tableSize = 1 << tableLog;
            int tableMask = tableSize - 1;
            int step = (tableSize >> 1) + (tableSize >> 3) + 3;

            var cumulativeCounts = new List<int>(new int[maxSymbolValue + 2]);
            var symbolTable = new List<int>(new int[tableSize]);

            // Fill cumulativeCounts
            for (int symbol = 1; symbol <= maxSymbolValue + 1; symbol++)
            {
                cumulativeCounts[symbol] = cumulativeCounts[symbol - 1] + normalizedCounts[symbol - 1];
            }

            // symbolTable
            int position = 0;
            for (int symbol = 0; symbol <= maxSymbolValue; symbol++)
            {
                int occurrences = normalizedCounts[symbol];
                for (int i = 0; i < occurrences; i++)
                {
                    symbolTable[position] = symbol;
                    position = (position + step) & tableMask;
                }
            }

            // stateTable
            cTable.StateTable = new List<ushort>(new ushort[tableSize]);
            for (int i = 0; i < tableSize; i++)
            {
                int symbol = symbolTable[i];
                cTable.StateTable[cumulativeCounts[symbol]++] = (ushort)(tableSize + i);
            }

            // symbolTransformTable
            cTable.SymbolTransformTable = new List<SymbolCompressionTransform>(new SymbolCompressionTransform[maxSymbolValue + 1]);
            int total = 0;
            for (int symbol = 0; symbol <= maxSymbolValue; symbol++)
            {
                if (normalizedCounts[symbol] > 0)
                {
                    int maxBitsOut = tableLog - GetHighestBitPosition((uint)(normalizedCounts[symbol] - 1));
                    int minStatePlus = normalizedCounts[symbol] << maxBitsOut;

                    cTable.SymbolTransformTable[symbol] = new SymbolCompressionTransform
                    {
                        StateBasedBitsOut = (uint)((maxBitsOut << 16) - minStatePlus),
                        MaxBitsOut = (uint)maxBitsOut,
                        MinStatePlus = (uint)minStatePlus,
                        NextStateOffset = (uint)(total - normalizedCounts[symbol])
                    };

                    total += normalizedCounts[symbol];
                }
            }

            return cTable;
        }

        private static void WriteNormalizedCounts(List<byte> compressed, List<short> normalizedCounts, int maxSymbolValue, int tableLog)
        {
            compressed.Add((byte)(tableLog - MinTableLog));
            compressed.Add((byte)maxSymbolValue);

            for (int symbol = 0; symbol <= maxSymbolValue; symbol++)
            {
                if (normalizedCounts[symbol] == 0) continue;

                int count = normalizedCounts[symbol];
                compressed.Add((byte)symbol);
                if (tableLog > 8)
                {
                    compressed.Add((byte)(count >> 8));
                    compressed.Add((byte)(count & 0xFF));
                }
                else
                {
                    compressed.Add((byte)count);
                }
            }
        }

        private static void PrintNormalizedCounter(List<short> normalizedCounts)
        {
            Console.WriteLine("Normalized Counter (non zero):");
            for (int i = 0; i < normalizedCounts.Count; i++)
            {
                if (normalizedCounts[i] != 0)
                {
                    Console.WriteLine($"Symbol {i} '{(char)i}': {normalizedCounts[i]}");
                }
            }
            Console.WriteLine();
        }

        private static void CompressDataUsingCTable(List<byte> compressed, List<byte> input, CTable cTable)
        {
            int inputSize = input.Count;
            int encodingPosition = inputSize;

            ulong bitContainer = 0;
            int bitPos = 0;

            void FlushBits()
            {
                while (bitPos >= 8)
                {
                    compressed.Add((byte)(bitContainer & 0xFF));
                    bitContainer >>= 8;
                    bitPos -= 8;
                }
            }

            void OutputBits(uint cState, int numberOfBitsToOutput)
            {
                uint bitMask = (1U << numberOfBitsToOutput) - 1;
                bitContainer |= (ulong)(cState & bitMask) << bitPos;
                bitPos += numberOfBitsToOutput;
            }

            void EncodeSymbol(ref uint cState, byte nextSymbol)
            {
                var nextSymbolTransformInfo = cTable.SymbolTransformTable[nextSymbol];

                uint numberOfBitsToOutput = (cState + nextSymbolTransformInfo.StateBasedBitsOut) >> 16;
                OutputBits(cState, (int)numberOfBitsToOutput);

                cState = cTable.StateTable[(cState >> (int)numberOfBitsToOutput) + nextSymbolTransformInfo.NextStateOffset];
            }

            void InitCState(out uint cState, byte nextSymbol)
            {
                var nextSymbolTransformInfo = cTable.SymbolTransformTable[nextSymbol];
                uint numberOfBitsToOutput = (nextSymbolTransformInfo.StateBasedBitsOut + (1 << 15)) >> 16;
                cState = (numberOfBitsToOutput << 16) - nextSymbolTransformInfo.StateBasedBitsOut;
                cState = cTable.StateTable[(cState >> (int)numberOfBitsToOutput) + nextSymbolTransformInfo.NextStateOffset];
            }

            uint CState1, CState2;

            // Handle odd input size
            if ((inputSize & 1) != 0)
            {
                InitCState(out CState1, input[--encodingPosition]);
                InitCState(out CState2, input[--encodingPosition]);
                EncodeSymbol(ref CState1, input[--encodingPosition]);
                FlushBits();
            }
            else
            {
                InitCState(out CState2, input[--encodingPosition]);
                InitCState(out CState1, input[--encodingPosition]);
            }
            inputSize -= 2;

            // Ensure the rest number of symbols % 4 == 0
            if ((sizeof(ulong) * 8 > cTable.TableLog * 4 + 7) && (inputSize & 2) != 0)
            {
                EncodeSymbol(ref CState2, input[--encodingPosition]);
                EncodeSymbol(ref CState1, input[--encodingPosition]);
                FlushBits();
            }

            while (encodingPosition > 3)
            {
                EncodeSymbol(ref CState2, input[--encodingPosition]);

                if (sizeof(ulong) * 8 < cTable.TableLog * 2 + 7)
                {
                    FlushBits();
                }

                EncodeSymbol(ref CState1, input[--encodingPosition]);

                if (sizeof(ulong) * 8 > cTable.TableLog * 4 + 7)
                {
                    EncodeSymbol(ref CState2, input[--encodingPosition]);
                    EncodeSymbol(ref CState1, input[--encodingPosition]);
                }
                FlushBits();
            }

            OutputBits(CState2, cTable.TableLog);
            OutputBits(CState1, cTable.TableLog);

            OutputBits(1, 1); // End mark
            FlushBits();

            if (bitPos > 0)
            {
                compressed.Add((byte)(bitContainer & 0xFF));
            }
        }

        public static List<byte> Compress(List<byte> input, bool verbose = false)
        {
            var compressed = new List<byte>();

            int maxSymbolValue = DefaultMaxSymbolValue;
            var count = CountSymbols(ref maxSymbolValue, input);

            int optimalTableLog = GetOptimalTableLog(DefaultTableLog, input.Count, maxSymbolValue);
            var normalizedCounts = NormalizeCount(optimalTableLog, count, input.Count, ref maxSymbolValue);

            var cTable = BuildCTable(normalizedCounts, maxSymbolValue, optimalTableLog);

            CompressDataUsingCTable(compressed, input, cTable);

            return compressed;
        }

        // ======================== Decompression ========================

        private static List<short> ReadNormalizedCount(
            ref int maxSymbolValue,
            ref int tableLog,
            List<byte> compressed,
            ref int normalizeCounterOffset)
        {
            tableLog = compressed[normalizeCounterOffset++] + MinTableLog;
            maxSymbolValue = compressed[normalizeCounterOffset++];
            var normalizedCounts = new List<short>(new short[maxSymbolValue + 1]);

            ushort maxCount = (ushort)((tableLog <= 8) ? 0xFF : 0xFFFF);

            for (int symbol = 0; symbol <= maxSymbolValue; symbol++)
            {
                symbol = compressed[normalizeCounterOffset++];
                if (tableLog > 8)
                {
                    normalizedCounts[symbol] = (short)((compressed[normalizeCounterOffset] << 8) + compressed[normalizeCounterOffset + 1]);
                    normalizeCounterOffset += 2;
                }
                else
                {
                    normalizedCounts[symbol] = (short)compressed[normalizeCounterOffset++];
                }
            }

            return normalizedCounts;
        }

        private static void BuildDTable(
            List<short> normalizedCounts,
            int maxSymbolValue,
            int tableLog,
            List<SymbolDecompressionTransform> dTable)
        {
            int tableSize = 1 << tableLog;
            dTable.Clear();
            dTable.AddRange(Enumerable.Repeat(new SymbolDecompressionTransform(), tableSize));

            int highThreshold = tableSize - 1;
            var symbolNext = new List<ushort>(new ushort[maxSymbolValue + 1]);

            for (int symbol = 0; symbol <= maxSymbolValue; symbol++)
            {
                if (normalizedCounts[symbol] == -1)
                {
                    dTable[highThreshold--].Symbol = (byte)symbol;
                    symbolNext[symbol] = 1;
                }
                else
                {
                    symbolNext[symbol] = (ushort)normalizedCounts[symbol];
                }
            }

            int step = (tableSize >> 1) + (tableSize >> 3) + 3;
            int position = 0;
            int tableMask = tableSize - 1;

            for (int symbol = 0; symbol <= maxSymbolValue; symbol++)
            {
                int occurrences = normalizedCounts[symbol];
                for (int i = 0; i < occurrences; i++)
                {
                    dTable[position].Symbol = (byte)symbol;
                    position = (position + step) & tableMask;
                    while (position > highThreshold)
                    {
                        position = (position + step) & tableMask;
                    }
                }
            }

            for (int index = 0; index < tableSize; index++)
            {
                byte symbol = dTable[index].Symbol;
                ushort nextState = symbolNext[symbol]++;
                dTable[index].NumberOfBitsToRead = (byte)(tableLog - GetHighestBitPosition(nextState));
                dTable[index].NewState = (ushort)((nextState << dTable[index].NumberOfBitsToRead) - tableSize);
            }
        }

        private static List<byte> DecompressDataUsingDTable(
            List<byte> compressed,
            List<SymbolDecompressionTransform> dTable,
            int tableLog,
            int maxSymbolValue,
            int normalizeCounterOffset)
        {
            var decompressed = new List<byte>();

            ulong bitContainer = 0;
            int bitPos = 0;
            int decodingPosition = compressed.Count - 1;

            void InitBitContainer()
            {
                while (decodingPosition >= normalizeCounterOffset && bitPos < sizeof(ulong) * 8)
                {
                    bitContainer <<= 8;
                    bitContainer |= compressed[decodingPosition--];
                    bitPos += 8;
                }
                while ((bitContainer >> bitPos) != 1) bitPos--;
            }

            void ReloadBitContainer()
            {
                while (decodingPosition >= normalizeCounterOffset && bitPos < sizeof(ulong) * 8 - 8)
                {
                    bitContainer <<= 8;
                    bitContainer |= compressed[decodingPosition--];
                    bitPos += 8;
                }
            }

            uint ReadBits(int numberOfBits)
            {
                uint value = (uint)((bitContainer >> (bitPos - numberOfBits)) & ((1U << numberOfBits) - 1));
                bitPos -= numberOfBits;
                return value;
            }

            byte DecodeSymbol(ref uint dState)
            {
                byte decodedSymbol = dTable[(int)dState].Symbol;
                uint lowBits = ReadBits(dTable[(int)dState].NumberOfBitsToRead);
                dState = (uint)(dTable[(int)dState].NewState + lowBits);
                return decodedSymbol;
            }

            InitBitContainer();

            uint state1 = ReadBits(tableLog);
            uint state2 = ReadBits(tableLog);

            while (decodingPosition >= normalizeCounterOffset)
            {
                decompressed.Add(DecodeSymbol(ref state1));
                decompressed.Add(DecodeSymbol(ref state2));
                ReloadBitContainer();
            }

            while (bitPos > 0)
            {
                decompressed.Add(DecodeSymbol(ref state1));
                if (bitPos == 0)
                {
                    decompressed.Add(dTable[(int)state2].Symbol);
                    decompressed.Add(dTable[(int)state1].Symbol);
                    break;
                }
                decompressed.Add(DecodeSymbol(ref state2));
                if (bitPos == 0)
                {
                    decompressed.Add(dTable[(int)state1].Symbol);
                    decompressed.Add(dTable[(int)state2].Symbol);
                }
            }

            return decompressed;
        }

        public static List<byte> Decompress(List<byte> input, bool verbose = false)
        {
            int maxSymbolValue = DefaultMaxSymbolValue;
            int tableLog = 0;
            int normalizeCounterOffset = 0;

            var normalizedCounts = ReadNormalizedCount(ref maxSymbolValue, ref tableLog, input, ref normalizeCounterOffset);

            if (verbose)
            {
                Console.WriteLine($"Table Log: {tableLog}");
                Console.WriteLine($"Max Symbol Value: {maxSymbolValue}");
                PrintNormalizedCounter(normalizedCounts);
            }

            var dTable = new List<SymbolDecompressionTransform>();
            BuildDTable(normalizedCounts, maxSymbolValue, tableLog, dTable);

            if (verbose)
            {
                Console.WriteLine("Decode Table:");
                Console.WriteLine("Index | New State | Symbol | Number of bits to read");
                Console.WriteLine("----------------------------------------------");
                for (int i = 0; i < dTable.Count; i++)
                {
                    var entry = dTable[i];
                    Console.WriteLine($"{i,5} | {entry.NewState,9} | {entry.Symbol,6} | {entry.NumberOfBitsToRead,7}");
                }
            }

            return DecompressDataUsingDTable(input, dTable, tableLog, maxSymbolValue, normalizeCounterOffset);
        }
    }
}