import struct
import sys
from PIL import Image

def convert(input_path, output_path):
    try:
        img = Image.open(input_path).convert('RGB')
        width, height = img.size
    except Exception as e:
        print(f"Failed to load image: {e}")
        return

    with open(output_path, 'wb') as f:
        f.write(struct.pack('<III', 0x49504449, width, height))
        
        for y in range(height):
            for x in range(width):
                r, g, b = img.getpixel((x, y))
                pixel = (r << 16) | (g << 8) | b 
                f.write(struct.pack('<I', pixel))
                
    print(f"Converted {input_path} -> {output_path} ({width}x{height})")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python3 png_to_idpimg.py <input.png> <output.idpimg>")
        sys.exit(1)
        
    convert(sys.argv[1], sys.argv[2])