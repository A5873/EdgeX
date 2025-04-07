# EdgeX Logo Artwork Specifications

This document provides detailed specifications for the creation and use of the EdgeX logo assets, ensuring consistent application across all platforms and materials.

## 1. Logo Variants

### 1.1 Full Logo (Symbol + Wordmark)

The primary logo consists of the hexagonal node network symbol with an integrated "X" alongside the EdgeX wordmark.

#### Measurements and Proportions
- Aspect ratio: 3:1 (width to height)
- Symbol should occupy approximately 20% of the total width
- Clearspace: Equal to the height of the lowercase "e" in the wordmark on all sides
- Symbol to wordmark spacing: Equal to 50% of the symbol's width

#### Hierarchy
- The symbol should be positioned to the left of the wordmark
- Baseline alignment: The bottom of the symbol should align with the baseline of the wordmark

### 1.2 Symbol Only

The standalone symbol is used for favicon, app icons, and applications with limited space.

#### Measurements and Proportions
- Square format (1:1 aspect ratio)
- Minimum size: 24px for digital, 0.5 inches for print
- Clearspace: 10% of the symbol's width on all sides

### 1.3 Wordmark Only

The standalone wordmark is used for text-dominant contexts.

#### Measurements and Proportions
- Aspect ratio: Approximately 4:1 (width to height)
- Minimum size: 100px for digital, 1.25 inches for print
- Clearspace: Equal to the height of the lowercase "e" in the wordmark on all sides

## 2. Color Specifications

### 2.1 Primary Logo Colors

**Full-color version (standard use):**
- Symbol: Deep Quantum Blue (#0B2447) with Edge Teal (#19A7CE) accents
- Wordmark: Deep Quantum Blue (#0B2447)

**Reversed version (for dark backgrounds):**
- Symbol: Quantum White (#F8FAFC) with Edge Teal (#19A7CE) accents
- Wordmark: Quantum White (#F8FAFC)

**Monochrome version:**
- Symbol and Wordmark: 100% Deep Quantum Blue (#0B2447)
- For grayscale applications: Symbol and Wordmark: 100% black (#000000)

### 2.2 Color Application

- The Edge Gradient (linear gradient from Deep Quantum Blue #0B2447 to Edge Teal #19A7CE) may be applied to the symbol for special applications
- Symbol node points should use Edge Teal (#19A7CE) for accent
- Main symbol structure should use Deep Quantum Blue (#0B2447)

## 3. File Format Requirements

### 3.1 Vector Formats

**SVG Format:**
- Optimized for web use
- Preserve paths, not outlines
- Viewbox set to natural artwork dimensions
- Ensure consistent path direction
- Non-zero fill rule

**AI Format (Adobe Illustrator):**
- Master artwork with layers intact
- All text converted to outlines
- CMYK and RGB color variants
- Artboards sized appropriately for each variant

### 3.2 Raster Formats

**PNG Format:**
- Transparent background
- Resolution: 72dpi for digital use
- Multiple sizes:
  - Full Logo: 300px, 600px, 900px widths
  - Symbol Only: 32px, 64px, 128px, 256px, 512px widths
  - Wordmark Only: 300px, 600px widths

**JPG Format (for print use):**
- Resolution: 300dpi
- CMYK color space
- White background
- Full Logo: 3 inches, 6 inches widths
- Symbol Only: 1 inch, 2 inches widths

## 4. Design Elements Specifications

### 4.1 Symbol Design

The hexagonal node network symbol represents edge computing and distributed systems:

- Six interconnected nodes arranged in a hexagonal pattern
- Node connections represented by clean, minimal strokes (2px weight at 64px size)
- Integrated stylized "X" in negative space at the center
- Rounded terminals (radius equal to stroke width)
- Node points slightly enlarged (3px radius at 64px size)
- Precision geometry with perfect 60° angles for hexagonal structure

### 4.2 Wordmark Design

The EdgeX wordmark uses a modified geometric sans-serif typeface:

- Custom letterforms based on a geometric sans-serif foundation
- Mixed case: uppercase "E" with lowercase "dgex"
- Letter spacing (tracking): +15 units
- "x" modified to echo design elements from the symbol
- Consistent stroke weight
- Slight customization of the "g" for improved recognition

## 5. Usage Guidelines

### 5.1 Logo Selection

**Use the Full Logo when:**
- Introducing the brand in new contexts
- In headers, footers, and primary brand placements
- Space permits the full horizontal format

**Use the Symbol Only when:**
- Space is limited (favicons, app icons, social media avatars)
- The brand is already established in context
- As a decorative element or icon
- In square format requirements

**Use the Wordmark Only when:**
- In text-dominant contexts
- Alongside other company wordmarks
- In narrow horizontal spaces
- When the brand is already established in context

### 5.2 Technical Implementation

**Minimum sizes:**
- Full Logo: 120px width for digital, 1.5 inches for print
- Symbol Only: 24px width for digital, 0.5 inches for print
- Wordmark Only: 100px width for digital, 1.25 inches for print

**Background considerations:**
- Prefer light backgrounds for standard logo variants
- Use reversed logo variants on dark backgrounds
- Ensure sufficient contrast between logo and background
- Avoid placing logo on busy patterns or photographs without sufficient contrast

### 5.3 Prohibited Usage

**Do not:**
- Stretch, compress, or distort the logo
- Rotate or tilt the logo
- Change the logo colors outside of approved variations
- Add effects such as shadows, bevels, or glows
- Place the logo on backgrounds with insufficient contrast
- Recreate, redraw, or modify the logo elements
- Change the spatial relationship between the symbol and wordmark
- Use outdated versions of the logo

### 5.4 File Naming Convention

Logo files should follow this naming convention:
- `edgex_[variant]_[color]_[size].[format]`

**Variants:**
- `full` (Symbol + Wordmark)
- `symbol` (Symbol Only)
- `wordmark` (Wordmark Only)

**Colors:**
- `color` (Standard color version)
- `reversed` (White for dark backgrounds)
- `mono` (Single color version)
- `gradient` (With gradient applied)

**Size:**
For PNG files only:
- Pixel dimensions for digital (e.g., `300px`)
- Inch dimensions for print (e.g., `3in`)

**Examples:**
- `edgex_full_color.svg`
- `edgex_symbol_reversed.svg`
- `edgex_wordmark_mono.svg`
- `edgex_full_color_600px.png`
- `edgex_symbol_gradient_128px.png`

## 6. Directory Structure

```
docs/assets/logo/
├── svg/
│   ├── full/
│   │   ├── edgex_full_color.svg
│   │   ├── edgex_full_reversed.svg
│   │   └── edgex_full_mono.svg
│   ├── symbol/
│   │   ├── edgex_symbol_color.svg
│   │   ├── edgex_symbol_reversed.svg
│   │   └── edgex_symbol_gradient.svg
│   └── wordmark/
│       ├── edgex_wordmark_color.svg
│       ├── edgex_wordmark_reversed.svg
│       └── edgex_wordmark_mono.svg
├── png/
│   ├── full/
│   │   ├── edgex_full_color_300px.png
│   │   ├── edgex_full_color_600px.png
│   │   ├── edgex_full_color_900px.png
│   │   └── ...
│   ├── symbol/
│   │   ├── edgex_symbol_color_32px.png
│   │   ├── edgex_symbol_color_64px.png
│   │   └── ...
│   └── wordmark/
│       ├── edgex_wordmark_color_300px.png
│       ├── edgex_wordmark_color_600px.png
│       └── ...
└── ai/
    ├── full/
    │   ├── edgex_full_rgb.ai
    │   └── edgex_full_cmyk.ai
    ├── symbol/
    │   ├── edgex_symbol_rgb.ai
    │   └── edgex_symbol_cmyk.ai
    └── wordmark/
        ├── edgex_wordmark_rgb.ai
        └── edgex_wordmark_cmyk.ai
```

## 7. Implementation Checklist

- [ ] Create master artwork in Adobe Illustrator
- [ ] Export SVG versions of all variants
- [ ] Generate PNG files at required sizes
- [ ] Prepare print-ready AI files in both RGB and CMYK
- [ ] Verify all clearspace and minimum size requirements
- [ ] Test logo legibility on various backgrounds
- [ ] Ensure consistent application of colors across all formats

