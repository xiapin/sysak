from lz4 import block
data = "hello lz4, hello lz4, hello lz4, hello lz4, hello lz4, hello lz4".encode()
compressed = block.compress(data, store_size=False)
print()
print(f'out/in: {len(compressed)}/{len(data)} Bytes')
print(f'Compression ratio: {len(compressed)/len(data):.2%}')