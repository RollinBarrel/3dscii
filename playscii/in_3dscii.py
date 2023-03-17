#TODO: add a hint about charset/palette lookup

import struct
from art_import import ArtImporter
from art import UV_NORMAL, UV_ROTATE90, UV_ROTATE180, UV_ROTATE270, UV_FLIPX, UV_FLIPY, UV_FLIP90, UV_FLIP270

class ThreeDSCIIImporter(ArtImporter):
    
    format_name = '3DSCII'
    format_description = """
Binary format used by 3DSCII.
    """
    
    def run_import(self, in_filename, options={}):
        data = open(in_filename, 'rb')

        if data.read(6).decode() != '3DSCII':
            return False
        
        revision = int.from_bytes(data.read(2), byteorder='little', signed=False)
        if revision != 4:
            return False
        
        nameSize = int.from_bytes(data.read(1), byteorder='little', signed=False)
        name = data.read(nameSize).decode()

        width = int.from_bytes(data.read(1), byteorder='little', signed=False)
        height = int.from_bytes(data.read(1), byteorder='little', signed=False)
        self.art.resize(width, height)

        charsetNameSize = int.from_bytes(data.read(1), byteorder='little', signed=False)
        charsetName = data.read(charsetNameSize).decode()
        self.set_art_charset(charsetName)

        paletteNameSize = int.from_bytes(data.read(1), byteorder='little', signed=False)
        paletteName = data.read(paletteNameSize).decode()
        self.set_art_palette(paletteName)
        print(charsetName)
        print(paletteName)

        layers = int.from_bytes(data.read(1), byteorder='little', signed=False)

        #TODO: support multiple layers
        self.art.clear_frame_layer(0, 0)

        for layer in range(0, layers):
            layerNameSize = int.from_bytes(data.read(1), byteorder='little', signed=False)
            layerName = data.read(layerNameSize).decode()

            #self.art.add_layer(1 / layers * layer, layerName)

            depthOffset = struct.unpack('f', data.read(4))
            alpha = struct.unpack('f', data.read(4))

            flags = int.from_bytes(data.read(1), byteorder='little', signed=False)
            visible = (flags & (1 << 0)) > 0

            x, y = 0, 0
            for i in range(0, width * height):
                value = int.from_bytes(data.read(2), byteorder='little', signed=False)
                fgColor = int.from_bytes(data.read(1), byteorder='little', signed=False)
                bgColor = int.from_bytes(data.read(1), byteorder='little', signed=False)

                flags = int.from_bytes(data.read(1), byteorder='little', signed=False)
                xFlip = (flags & (1 << 0)) > 0
                yFlip = (flags & (1 << 1)) > 0
                dFlip = (flags & (1 << 2)) > 0
                invColors = (flags & (1 << 3)) > 0

                transform = UV_NORMAL
                if dFlip:
                    if yFlip:
                        if xFlip:
                            transform = UV_FLIP270
                        else:
                            transform = UV_ROTATE90
                    else:
                        if xFlip:
                            transform = UV_ROTATE270
                        else:
                            transform = UV_FLIP90
                else:
                    if yFlip:
                        if xFlip:
                            transform = UV_ROTATE180
                        else:
                            transform = UV_FLIPY
                    else:
                        if xFlip:
                            transform = UV_FLIPX

                #TODO: out of range tiles/colors

                self.art.set_tile_at(0, 0, x, y, value, fgColor, bgColor, transform)

                x += 1
                if (x >= width):
                    x = 0
                    y += 1

        return True
