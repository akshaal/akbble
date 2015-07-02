#!/usr/bin/python

import ImageFont, ImageDraw, Image

FONT_SIZE = 72
FONT_FILE_PATH = "font.ttf"

OUTPUT_IMAGE_FILEPATH_TEMPLATE = "num_%d_%s.png"

TILE_WIDTH_PIXELS = 144/3
TILE_HEIGHT_PIXELS = int(168.0/2.5)

LARGE_SCRATCH_CANVAS_DIMENSIONS = (100, 100)
FINAL_TILE_CANVAS_DIMENSIONS = (TILE_WIDTH_PIXELS, TILE_HEIGHT_PIXELS)

META_DATA_TEMPLATE = \
"""
        {
        "type": "png",
        "name": "IMAGE_NUM_%d_%s",
        "file": "images/num_%d_%s.png"
        }"""


meta_data_entries = []

font = ImageFont.truetype(FONT_FILE_PATH, FONT_SIZE)


if __name__ == "__main__":
    # Generate the image tile file for each digit.
    for clr in ['d', 'b']:
        for digit in range(0, 12):
            if clr == 'd': fill=(0,0,0,255)
            else: fill=(0, 0, 170,255)

            # Draw the digit on a large canvas so PIL doesn't crop it.
            scratch_canvas_image = Image.new("RGB", size = LARGE_SCRATCH_CANVAS_DIMENSIONS)
            scratch_canvas_image2 = Image.new("RGB", size = LARGE_SCRATCH_CANVAS_DIMENSIONS, color = fill)
            draw = ImageDraw.Draw(scratch_canvas_image)
            draw2 = ImageDraw.Draw(scratch_canvas_image2)

            if digit == 10: x = 'A'
            elif digit == 11: x = 'B'
            else: x = str(digit)

            draw.text((0,0), x, font=font)
            draw2.text((0,0), x, font=font)

            # Discard all the padding
            cropped_digit_image = scratch_canvas_image2.crop(scratch_canvas_image.getbbox())

            # Center the digit within the final image tile and save it
            digit_width, digit_height = cropped_digit_image.size

            tile_image = Image.new("RGB", size = FINAL_TILE_CANVAS_DIMENSIONS, color=fill)

            tile_image.paste(cropped_digit_image, ((TILE_WIDTH_PIXELS-digit_width)/2, (TILE_HEIGHT_PIXELS-digit_height)/2))

            tile_image.save(OUTPUT_IMAGE_FILEPATH_TEMPLATE % (digit, clr))

            meta_data_entries.append(META_DATA_TEMPLATE % (digit, clr, digit, clr))
    # Display the meta data which needs to be included in `resource_map.json`.
    print ",\n".join(meta_data_entries)


