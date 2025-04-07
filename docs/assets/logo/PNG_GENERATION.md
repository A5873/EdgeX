# EdgeX Logo PNG Generation Guide

This document provides instructions for generating PNG files from the EdgeX SVG logo assets at the required sizes and quality settings.

## Required Tools

Either of the following tools can be used for SVG to PNG conversion:

- **Inkscape** (preferred): Provides excellent SVG rendering and command-line capabilities
  - Installation: https://inkscape.org/release/
  - Command-line interface: `inkscape` 

- **ImageMagick**: Alternative option for conversion 
  - Installation: https://imagemagick.org/script/download.php
  - Command-line interface: `convert`

## Required PNG Files

The following PNG files should be generated from the SVG source files:

### Full Logo
Source: `svg/full/edgex_full_color.svg` and `svg/full/edgex_full_reversed.svg`

#### Color Version
- `png/full/edgex_full_color_300.png` (300px width)
- `png/full/edgex_full_color_600.png` (600px width)
- `png/full/edgex_full_color_900.png` (900px width)

#### Reversed Version (for dark backgrounds)
- `png/full/edgex_full_reversed_300.png` (300px width)
- `png/full/edgex_full_reversed_600.png` (600px width)
- `png/full/edgex_full_reversed_900.png` (900px width)

### Symbol
Source: `svg/symbol/edgex_symbol_color.svg` and `svg/symbol/edgex_symbol_reversed.svg`

#### Color Version
- `png/symbol/edgex_symbol_color_32.png` (32px width)
- `png/symbol/edgex_symbol_color_64.png` (64px width)
- `png/symbol/edgex_symbol_color_128.png` (128px width)
- `png/symbol/edgex_symbol_color_256.png` (256px width)
- `png/symbol/edgex_symbol_color_512.png` (512px width)

#### Reversed Version (for dark backgrounds)
- `png/symbol/edgex_symbol_reversed_32.png` (32px width)
- `png/symbol/edgex_symbol_reversed_64.png` (64px width)
- `png/symbol/edgex_symbol_reversed_128.png` (128px width)
- `png/symbol/edgex_symbol_reversed_256.png` (256px width)
- `png/symbol/edgex_symbol_reversed_512.png` (512px width)

### Wordmark
Source: `svg/wordmark/edgex_wordmark_color.svg` and `svg/wordmark/edgex_wordmark_reversed.svg`

#### Color Version
- `png/wordmark/edgex_wordmark_color_300.png` (300px width)
- `png/wordmark/edgex_wordmark_color_600.png` (600px width)

#### Reversed Version (for dark backgrounds)
- `png/wordmark/edgex_wordmark_reversed_300.png` (300px width)
- `png/wordmark/edgex_wordmark_reversed_600.png` (600px width)

## Example Commands

### Using Inkscape

```bash
# Create directories if they don't exist
mkdir -p png/{full,symbol,wordmark}

# Full Logo - Color
inkscape -w 300 -h 100 svg/full/edgex_full_color.svg -o png/full/edgex_full_color_300.png
inkscape -w 600 -h 200 svg/full/edgex_full_color.svg -o png/full/edgex_full_color_600.png
inkscape -w 900 -h 300 svg/full/edgex_full_color.svg -o png/full/edgex_full_color_900.png

# Full Logo - Reversed
inkscape -w 300 -h 100 svg/full/edgex_full_reversed.svg -o png/full/edgex_full_reversed_300.png
inkscape -w 600 -h 200 svg/full/edgex_full_reversed.svg -o png/full/edgex_full_reversed_600.png
inkscape -w 900 -h 300 svg/full/edgex_full_reversed.svg -o png/full/edgex_full_reversed_900.png

# Symbol - Color
inkscape -w 32 -h 32 svg/symbol/edgex_symbol_color.svg -o png/symbol/edgex_symbol_color_32.png
inkscape -w 64 -h 64 svg/symbol/edgex_symbol_color.svg -o png/symbol/edgex_symbol_color_64.png
inkscape -w 128 -h 128 svg/symbol/edgex_symbol_color.svg -o png/symbol/edgex_symbol_color_128.png
inkscape -w 256 -h 256 svg/symbol/edgex_symbol_color.svg -o png/symbol/edgex_symbol_color_256.png
inkscape -w 512 -h 512 svg/symbol/edgex_symbol_color.svg -o png/symbol/edgex_symbol_color_512.png

# Symbol - Reversed
inkscape -w 32 -h 32 svg/symbol/edgex_symbol_reversed.svg -o png/symbol/edgex_symbol_reversed_32.png
inkscape -w 64 -h 64 svg/symbol/edgex_symbol_reversed.svg -o png/symbol/edgex_symbol_reversed_64.png
inkscape -w 128 -h 128 svg/symbol/edgex_symbol_reversed.svg -o png/symbol/edgex_symbol_reversed_128.png
inkscape -w 256 -h 256 svg/symbol/edgex_symbol_reversed.svg -o png/symbol/edgex_symbol_reversed_256.png
inkscape -w 512 -h 512 svg/symbol/edgex_symbol_reversed.svg -o png/symbol/edgex_symbol_reversed_512.png

# Wordmark - Color
inkscape -w 300 -h 75 svg/wordmark/edgex_wordmark_color.svg -o png/wordmark/edgex_wordmark_color_300.png
inkscape -w 600 -h 150 svg/wordmark/edgex_wordmark_color.svg -o png/wordmark/edgex_wordmark_color_600.png

# Wordmark - Reversed
inkscape -w 300 -h 75 svg/wordmark/edgex_wordmark_reversed.svg -o png/wordmark/edgex_wordmark_reversed_300.png
inkscape -w 600 -h 150 svg/wordmark/edgex_wordmark_reversed.svg -o png/wordmark/edgex_wordmark_reversed_600.png
```

### Using ImageMagick

```bash
# Create directories if they don't exist
mkdir -p png/{full,symbol,wordmark}

# Full Logo - Color
convert -background none -density 300 -resize 300x svg/full/edgex_full_color.svg png/full/edgex_full_color_300.png
convert -background none -density 300 -resize 600x svg/full/edgex_full_color.svg png/full/edgex_full_color_600.png
convert -background none -density 300 -resize 900x svg/full/edgex_full_color.svg png/full/edgex_full_color_900.png

# Full Logo - Reversed
convert -background none -density 300 -resize 300x svg/full/edgex_full_reversed.svg png/full/edgex_full_reversed_300.png
convert -background none -density 300 -resize 600x svg/full/edgex_full_reversed.svg png/full/edgex_full_reversed_600.png
convert -background none -density 300 -resize 900x svg/full/edgex_full_reversed.svg png/full/edgex_full_reversed_900.png

# Symbol - Color
convert -background none -density 300 -resize 32x32 svg/symbol/edgex_symbol_color.svg png/symbol/edgex_symbol_color_32.png
convert -background none -density 300 -resize 64x64 svg/symbol/edgex_symbol_color.svg png/symbol/edgex_symbol_color_64.png
convert -background none -density 300 -resize 128x128 svg/symbol/edgex_symbol_color.svg png/symbol/edgex_symbol_color_128.png
convert -background none -density 300 -resize 256x256 svg/symbol/edgex_symbol_color.svg png/symbol/edgex_symbol_color_256.png
convert -background none -density 300 -resize 512x512 svg/symbol/edgex_symbol_color.svg png/symbol/edgex_symbol_color_512.png

# Symbol - Reversed
convert -background none -density 300 -resize 32x32 svg/symbol/edgex_symbol_reversed.svg png/symbol/edgex_symbol_reversed_32.png
convert -background none -density 300 -resize 64x64 svg/symbol/edgex_symbol_reversed.svg png/symbol/edgex_symbol_reversed_64.png
convert -background none -density 300 -resize 128x128 svg/symbol/edgex_symbol_reversed.svg png/symbol/edgex_symbol_reversed_128.png
convert -background none -density 300 -resize 256x256 svg/symbol/edgex_symbol_reversed.svg png/symbol/edgex_symbol_reversed_256.png
convert -background none -density 300 -resize 512x512 svg/symbol/edgex_symbol_reversed.svg png/symbol/edgex_symbol_reversed_512.png

# Wordmark - Color
convert -background none -density 300 -resize 300x svg/wordmark/edgex_wordmark_color.svg png/wordmark/edgex_wordmark_color_300.png
convert -background none -density 300 -resize 600x svg/wordmark/edgex_wordmark_color.svg png/wordmark/edgex_wordmark_color_600.png

# Wordmark - Reversed
convert -background none -density 300 -resize 300x svg/wordmark/edgex_wordmark_reversed.svg png/wordmark/edgex_wordmark_reversed_300.png
convert -background none -density 300 -resize 600x svg/wordmark/edgex_wordmark_reversed.svg png/wordmark/edgex_wordmark_reversed_600.png
```

## Quality and Optimization Requirements

All PNG files should meet the following requirements:

1. **Transparent Background**: All logos should have transparent backgrounds (`-background none` in ImageMagick)
2. **Anti-aliasing**: Ensure smooth edges by using appropriate density settings (`-density 300` in ImageMagick)
3. **Optimization**: After generation, all PNGs should be optimized to reduce file size while maintaining quality
   - Use tools like `pngquant`, `optipng`, or `ImageOptim`
   - Example: `find png -name "*.png" -exec optipng -o5 {} \;`
4. **Color Accuracy**: The colors should exactly match the SVG source:
   - Deep Quantum Blue: #0B2447
   - Edge Teal: #19A7CE
   - Quantum White: #F8FAFC

## Directory Structure

The final structure should be:

```
docs/assets/logo/
├── svg/
│   ├── full/
│   │   ├── edgex_full_color.svg
│   │   └── edgex_full_reversed.svg
│   ├── symbol/
│   │   ├── edgex_symbol_color.svg
│   │   └── edgex_symbol_reversed.svg
│   └── wordmark/
│       ├── edgex_wordmark_color.svg
│       └── edgex_wordmark_reversed.svg
├── png/
│   ├── full/
│   │   ├── edgex_full_color_300.png
│   │   ├── edgex_full_color_600.png
│   │   ├── edgex_full_color_900.png
│   │   ├── edgex_full_reversed_300.png
│   │   ├── edgex_full_reversed_600.png
│   │   └── edgex_full_reversed_900.png
│   ├── symbol/
│   │   ├── edgex_symbol_color_32.png
│   │   ├── edgex_symbol_color_64.png
│   │   ├── edgex_symbol_color_128.png
│   │   ├── edgex_symbol_color_256.png
│   │   ├── edgex_symbol_color_512.png
│   │   ├── edgex_symbol_reversed_32.png
│   │   ├── edgex_symbol_reversed_64.png
│   │   ├── edgex_symbol_reversed_128.png
│   │   ├── edgex_symbol_reversed_256.png
│   │   └── edgex_symbol_reversed_512.png
│   └── wordmark/
│       ├── edgex_wordmark_color_300.png
│       ├── edgex_wordmark_color_600.png
│       ├── edgex_wordmark_reversed_300.png
│       └── edgex_wordmark_reversed_600.png
└── PNG_GENERATION.md
```

## Testing and Verification

After generating the PNG files, verify the following:

1. **All Files Present**: Check that all 22 PNG files have been generated with the correct names
2. **Dimensions Correct**: Verify each file has the correct dimensions using:
   ```
   identify -format "%f: %wx%h\n" png/*/*.png | sort
   ```
3. **Transparency**: Check each PNG on both light and dark backgrounds to ensure transparency is working correctly
4. **Color Accuracy**: Verify the colors match the specifications, especially between color and reversed versions
5. **Visual Quality**: Ensure there are no rendering artifacts, especially at smaller sizes
6. **File Size**: Check that files are reasonably optimized - each should be no more than a few KB (for symbol) to a few hundred KB (for larger full logos)

## Common Issues and Troubleshooting

- **Blurry Edges**: Increase the density in ImageMagick (e.g., from 300 to 600)
- **Unwanted Background**: Ensure the `-background none` parameter is used
- **Incorrect Colors**: Verify the SVG source files have the correct color values
- **Poor Small Icon Quality**: For smaller icons (especially 32px), you may need to manually adjust the SVG for that size

