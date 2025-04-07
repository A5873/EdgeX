# EdgeX Logo Assets

This directory contains the official EdgeX logo assets in various formats and variants for use across digital and print applications.

## Overview

The EdgeX logo system consists of three main components:

1. **Symbol** - The hexagonal network icon that represents EdgeX's connectivity and edge computing focus
2. **Wordmark** - The "EdgeX" text representation of the brand name
3. **Full Logo** - The combined symbol and wordmark in their proper relationship

Each component is available in:
- **Color** - Standard version using the EdgeX brand colors
- **Reversed** - White version for use on dark backgrounds

## Directory Structure

```
docs/assets/logo/
├── svg/             # SVG vector source files
│   ├── full/        # Full logo (symbol + wordmark)
│   ├── symbol/      # Symbol only
│   └── wordmark/    # Wordmark only
├── png/             # PNG raster files at various sizes
│   ├── full/        # Full logo at 300px, 600px, 900px widths
│   ├── symbol/      # Symbol at 32px, 64px, 128px, 256px, 512px
│   └── wordmark/    # Wordmark at 300px, 600px widths
├── ai/              # Adobe Illustrator source files (for designers)
├── PNG_GENERATION.md # Instructions for generating PNG files
└── README.md        # This file
```

## Logo Specifications

### Color Palette

- **Deep Quantum Blue** (#0B2447) - Primary brand color, used for wordmark and symbol structure
- **Edge Teal** (#19A7CE) - Accent color, used for node points in the symbol
- **Quantum White** (#F8FAFC) - Used for reversed versions on dark backgrounds

### Available Variants

| Component | Format | Variants | Sizes |
|-----------|--------|----------|-------|
| Full Logo | SVG | Color, Reversed | Scalable |
| Full Logo | PNG | Color, Reversed | 300px, 600px, 900px width |
| Symbol | SVG | Color, Reversed | Scalable |
| Symbol | PNG | Color, Reversed | 32px, 64px, 128px, 256px, 512px |
| Wordmark | SVG | Color, Reversed | Scalable |
| Wordmark | PNG | Color, Reversed | 300px, 600px width |

## Quick Start Guide

### Website/Digital Use Cases

- **Favicon**: Use `png/symbol/edgex_symbol_color_32.png` or `edgex_symbol_color_64.png`
- **Header Logo**: Use `svg/full/edgex_full_color.svg` (preferred) or `png/full/edgex_full_color_300.png`
- **Footer Logo**: Use `svg/symbol/edgex_symbol_color.svg` or `png/symbol/edgex_symbol_color_64.png`
- **Dark Mode**: Use reversed versions for dark backgrounds: `svg/full/edgex_full_reversed.svg`

### Documentation Use Cases

- **Cover Page**: Use `svg/full/edgex_full_color.svg` or `png/full/edgex_full_color_600.png`
- **Headers**: Use `svg/wordmark/edgex_wordmark_color.svg` or `png/wordmark/edgex_wordmark_color_300.png`
- **Diagrams**: Use `svg/symbol/edgex_symbol_color.svg` or `png/symbol/edgex_symbol_color_128.png`

### Social Media Use Cases

- **Profile Picture**: Use `png/symbol/edgex_symbol_color_512.png`
- **Banner Images**: Use `png/full/edgex_full_color_900.png`

## Generation and Modification

- SVG files are the source files and should be used whenever possible
- To generate PNG files from SVG, refer to [PNG_GENERATION.md](PNG_GENERATION.md)
- For design modifications, contact the EdgeX marketing team

## Usage Guidelines

1. **Do not modify** the logo colors or proportions
2. Maintain adequate clear space around the logo (minimum space equal to the height of the "E" in the wordmark)
3. Use the appropriate reversed version on dark backgrounds
4. For very small applications (under 32px), use only the symbol without the wordmark
5. Always use the official files provided in this repository

## Questions and Support

For questions about logo usage or to request additional formats, please contact the EdgeX marketing team or open an issue in the repository.

