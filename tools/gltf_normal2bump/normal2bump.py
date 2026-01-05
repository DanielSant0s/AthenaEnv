import os
import io
import argparse
import numpy as np
from PIL import Image
from pygltflib import (
    GLTF2, Image as GLTFImage, Texture, Material, PbrMetallicRoughness, TextureInfo
)
import base64

def normal_to_bump(normal_img: Image.Image) -> Image.Image:
    normal = np.asarray(normal_img).astype(np.float32) / 255.0
    bump = normal[..., 2]  
    bump = (bump - bump.min()) / (bump.max() - bump.min())  
    bump_img = (bump * 255).astype(np.uint8)

    grayscale_img = Image.fromarray(bump_img, mode="L")

    unique_colors = len(set(bump_img.flatten()))
    
    print(f"Unique colors in bump map: {unique_colors}")

    if unique_colors <= 16:
        print("Using PAL4 mode (4-bit palette)")

        palette_img = grayscale_img.quantize(colors=16, method=Image.Resampling.LANCZOS)

        if palette_img.mode != 'P':
            palette_img = palette_img.convert('P')
        return palette_img
    elif unique_colors <= 256:
        print("Using PAL8 mode (8-bit palette)")

        palette_img = grayscale_img.quantize(colors=min(unique_colors, 256), method=Image.Resampling.LANCZOS)
        if palette_img.mode != 'P':
            palette_img = palette_img.convert('P')
        return palette_img
    else:
        print("Too many colors, using PAL8 mode with 256 colors")
        palette_img = grayscale_img.quantize(colors=256, method=Image.Resampling.LANCZOS)
        if palette_img.mode != 'P':
            palette_img = palette_img.convert('P')
        return palette_img

def image_from_gltf_image(gltf: GLTF2, img: GLTFImage, gltf_path: str) -> Image.Image:
    if img.uri:
        if img.uri.startswith("data:"):
            base64_data = img.uri.split(",")[1]
            raw_bytes = base64.b64decode(base64_data)
            return Image.open(io.BytesIO(raw_bytes)).convert("RGB")
        else:
            img_path = os.path.join(os.path.dirname(gltf_path), img.uri)
            return Image.open(img_path).convert("RGB")
    elif img.bufferView is not None:
        buffer_view = gltf.bufferViews[img.bufferView]
        buffer = gltf.buffers[0] 
        buffer_data = base64.b64decode(buffer.uri.split(",")[1]) if buffer.uri.startswith("data:") else open(os.path.join(os.path.dirname(gltf_path), buffer.uri), "rb").read()
        img_bytes = buffer_data[buffer_view.byteOffset:buffer_view.byteOffset + buffer_view.byteLength]
        return Image.open(io.BytesIO(img_bytes)).convert("RGB")
    else:
        raise ValueError("Unsupported image")

def convert_gltf_normal_to_bump(gltf_path: str, output_path: str, bump_scale=1.0, output_glb=False):
    gltf = GLTF2().load(gltf_path)

    is_input_gltf = gltf_path.lower().endswith('.gltf')
    is_output_gltf = output_path.lower().endswith('.gltf')

    for i, material in enumerate(gltf.materials):
        if material.normalTexture is not None:
            tex_index = material.normalTexture.index
            img_index = gltf.textures[tex_index].source
            img_data = gltf.images[img_index]

            try:
                normal_img = image_from_gltf_image(gltf, img_data, gltf_path)
            except Exception as e:
                print(f"Error loading texture image: {e}")
                continue

            bump_img = normal_to_bump(normal_img)

            bump_filename = f"bump_map_{i}.png"

            if is_input_gltf or (not output_glb and is_output_gltf):
                bump_img.save(bump_filename)

                bump_gltf_image = GLTFImage(uri=f"{bump_filename}")
            else:
                bump_img.save(bump_filename)
                with open(bump_filename, "rb") as f:
                    encoded_bump = base64.b64encode(f.read()).decode("utf-8")
                bump_gltf_image = GLTFImage(uri=f"data:image/png;base64,{encoded_bump}")

                os.remove(bump_filename)

            bump_image_index = len(gltf.images)
            gltf.images.append(bump_gltf_image)

            bump_texture = Texture(source=bump_image_index)
            bump_texture_index = len(gltf.textures)
            gltf.textures.append(bump_texture)

            material.extensions = {
                "ATHENA_material_bump": {
                    "bumpTexture": {"index": bump_texture_index},
                    "bumpScale": bump_scale
                }
            }
            
    if output_glb:
        try:
            from pygltflib import ImageFormat
            gltf.convert_images(ImageFormat.DATAURI)
        except ImportError:
            pass  
        gltf.save_binary(output_path)
    else:
        gltf.save(output_path)

    print(f"File saved as {output_path}")

def main():
    parser = argparse.ArgumentParser(
        description='Convert normal textures from GLTF/GLB files to bump maps',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Usage examples:
  %(prog)s input.gltf output.gltf
  %(prog)s input.glb output.glb --glb
  %(prog)s model.gltf model_bump.gltf --scale 0.5
  %(prog)s model.glb model_bump.glb --glb --scale 2.0
        '''
    )
    
    parser.add_argument('input', 
                       help='Input GLTF/GLB file')
    
    parser.add_argument('output', 
                       help='Output GLTF/GLB file')
    
    parser.add_argument('--scale', '-s', 
                       type=float, 
                       default=1.0,
                       help='Bump map scale (default: 1.0)')
    
    parser.add_argument('--glb', 
                       action='store_true',
                       help='Force GLB output format (embedded textures)')

    args = parser.parse_args()

    if not os.path.exists(args.input):
        print(f"Error: Input file '{args.input}' not found.")
        return 1

    if not (args.input.lower().endswith('.gltf') or args.input.lower().endswith('.glb')):
        print("Error: Input file must be .gltf or .glb")
        return 1

    output_glb = args.glb or args.output.lower().endswith('.glb')

    try:
        convert_gltf_normal_to_bump(args.input, args.output, args.scale, output_glb)
        return 0
    except Exception as e:
        print(f"Error during conversion: {e}")
        return 1

if __name__ == "__main__":
    exit(main())