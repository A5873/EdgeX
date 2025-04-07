# EdgeX PNG Logo Assets

This directory contains the official EdgeX logo assets in PNG (Portable Network Graphics) format at various sizes for different applications.

## PNG Format Overview

PNG is a raster image format that supports lossless compression and transparency, making it suitable for logos and graphics where quality is important. Unlike SVG, PNG files have fixed dimensions and are appropriate for contexts where vector graphics are not supported or when specific pixel dimensions are required.

## Available Sizes and Use Cases

### Full Logo (Symbol + Wordmark)

| Size | Filename | Dimensions | Primary Use Cases |
|------|----------|------------|-------------------|
| 300px | `edgex_full_color_300.png` | 300×100px | Email signatures, small web headers |
| 600px | `edgex_full_color_600.png` | 600×200px | Standard web headers, presentations, documents |
| 900px | `edgex_full_color_900.png` | 900×300px | Large headers, banners, high-resolution displays |

### Symbol Only

| Size | Filename | Dimensions | Primary Use Cases |
|------|----------|------------|-------------------|
| 32px | `edgex_symbol_color_32.png` | 32×32px | Favicon, small app icons |
| 64px | `edgex_symbol_color_64.png` | 64×64px | Standard app icons, small website elements |
| 128px | `edgex_symbol_color_128.png` | 128×128px | Medium app icons, UI elements |
| 256px | `edgex_symbol_color_256.png` | 256×256px | Large app icons, promotional materials |
| 512px | `edgex_symbol_color_512.png` | 512×512px | App store icons, high-resolution applications |

### Wordmark Only

| Size | Filename | Dimensions | Primary Use Cases |
|------|----------|------------|-------------------|
| 300px | `edgex_wordmark_color_300.png` | 300×75px | Secondary headers, UI elements |
| 600px | `edgex_wordmark_color_600.png` | 600×150px | Large headers, banners |

All sizes are also available in reversed versions for use on dark backgrounds.

## File Naming Convention

PNG files follow this naming pattern:

`edgex_[component]_[color-variant]_[width].png`

Where:
- `component` is one of: `symbol`, `wordmark`, or `full`
- `color-variant` is one of: `color` or `reversed`
- `width` is the width in pixels (height is proportional)

Examples:
- `edgex_full_color_600.png` - Complete logo in standard colors at 600px width
- `edgex_symbol_reversed_64.png` - Symbol only in white at 64px width (for dark backgrounds)

## Quality Specifications

All PNG assets meet the following quality requirements:

- **Transparent Background**: All logos have transparent backgrounds
- **Color Accuracy**: Exact brand color matching (Deep Quantum Blue: #0B2447, Edge Teal: #19A7CE)
- **Anti-aliasing**: Smooth edges at all sizes
- **Optimization**: Compressed without quality loss
- **Alpha Channel**: Full 8-bit alpha transparency
- **Pixel Precision**: Sharp, clear rendering at native size

## When to Use PNG vs SVG

### Use PNG When:

- Working with platforms that don't support SVG (some email clients, older software)
- Creating social media assets where specific pixel dimensions are required
- Developing for environments with limited vector support
- Needing a favicon or app icon at exact dimensions
- Creating image assets for applications where vector rendering might be inconsistent

### Use SVG When:

- Developing for modern web platforms
- Creating responsive designs where the logo needs to scale
- Working with print materials that require scaling
- Adding the logo to applications where file size is a concern
- Implementing animations or interactive elements

## Optimization Guidelines

For optimal use of these PNG files:

1. **Choose the Right Size**: Select the closest size to your intended display size
2. **Preserve Transparency**: Ensure any background elements respect the transparent PNG background
3. **Don't Scale Up**: Never use a smaller PNG at a larger size than intended; use SVG or a larger PNG
4. **File Size Consideration**: When bandwidth is critical, use the smallest suitable dimension
5. **Color Profile**: These PNGs use the sRGB color profile for consistent display across devices

## Available PNG Variants and Recommended Applications

### Full Logo

- **Color Variants**
  - `full/edgex_full_color_300.png` - Email signatures, small website headers
  - `full/edgex_full_color_600.png` - Standard website headers, presentation slides
  - `full/edgex_full_color_900.png` - Large banners, marketing materials, high-resolution displays

- **Reversed Variants**
  - `full/edgex_full_reversed_300.png` - Email signatures for dark mode, small website dark headers
  - `full/edgex_full_reversed_600.png` - Standard website dark headers, dark-themed presentations
  - `full/edgex_full_reversed_900.png` - Large dark-themed banners and materials

### Symbol

- **Color Variants**
  - `symbol/edgex_symbol_color_32.png` - Favicon, tiny UI elements
  - `symbol/edgex_symbol_color_64.png` - Standard UI icons, small social media elements
  - `symbol/edgex_symbol_color_128.png` - App icons, medium UI elements
  - `symbol/edgex_symbol_color_256.png` - Large icons, high-resolution displays
  - `symbol/edgex_symbol_color_512.png` - App store listings, large marketing graphics

- **Reversed Variants**
  - `symbol/edgex_symbol_reversed_32.png` - Favicon for dark sites, tiny UI elements on dark backgrounds
  - `symbol/edgex_symbol_reversed_64.png` - Standard UI icons for dark themes
  - `symbol/edgex_symbol_reversed_128.png` - App icons for dark interfaces
  - `symbol/edgex_symbol_reversed_256.png` - Large icons for dark-themed materials
  - `symbol/edgex_symbol_reversed_512.png` - App store listings with dark backgrounds

### Wordmark

- **Color Variants**
  - `wordmark/edgex_wordmark_color_300.png` - Secondary headers, document footers
  - `wordmark/edgex_wordmark_color_600.png` - Primary headers, large document elements

- **Reversed Variants**
  - `wordmark/edgex_wordmark_reversed_300.png` - Secondary headers on dark backgrounds
  - `wordmark/edgex_wordmark_reversed_600.png` - Primary headers on dark backgrounds

## Generating PNG Files

If you need to generate the PNG files from SVG sources, please refer to the [PNG Generation Guide](../PNG_GENERATION.md) for detailed instructions.

## Questions and Support

For questions about PNG logo usage or to request additional sizes, please contact the EdgeX marketing team or open an issue in the repository.

