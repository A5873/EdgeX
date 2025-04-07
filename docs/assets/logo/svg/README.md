# EdgeX SVG Logo Assets

This directory contains the official EdgeX logo assets in SVG (Scalable Vector Graphics) format. SVG is the preferred format for digital applications due to its scalability and small file size.

## SVG Format Advantages

- **Infinitely Scalable**: SVG files maintain perfect quality at any size
- **Small File Size**: Typically smaller than raster formats like PNG
- **Accessibility**: Text within SVG can be read by screen readers
- **Programmability**: Can be animated or modified with CSS and JavaScript
- **Resolution Independence**: Renders perfectly on all screen densities (including retina/high-DPI)
- **Searchability**: Text content in SVG files can be indexed by search engines

## Use Cases

SVG files are ideal for:
- Websites and web applications
- Digital presentations
- Mobile applications
- Interactive displays
- Print materials (when vector formats are accepted)
- Anywhere the logo might need to be displayed at different sizes

## File Naming Convention

SVG files follow this naming pattern:

`edgex_[component]_[color-variant].svg`

Where:
- `component` is one of: `symbol`, `wordmark`, or `full`
- `color-variant` is one of: `color` or `reversed`

Examples:
- `edgex_full_color.svg` - Complete logo in standard colors
- `edgex_symbol_reversed.svg` - Symbol only in white (for dark backgrounds)

## Technical Specifications

### ViewBox and Dimensions

| Component | ViewBox | Width | Height |
|-----------|---------|-------|--------|
| Full Logo | "0 0 300 100" | 300px | 100px |
| Symbol | "0 0 64 64" | 64px | 64px |
| Wordmark | "0 0 200 50" | 200px | 50px |

### Colors

SVG files use the following color specifications:
- Deep Quantum Blue: `#0B2447`
- Edge Teal: `#19A7CE`
- Quantum White: `#F8FAFC`

### Structure

All SVG files include:
- Proper XML namespaces
- Title and description metadata for accessibility
- CSS classes for styling components
- Clean, optimized paths with meaningful names

## Best Practices for SVG Usage

1. **Prefer SVG over PNG** whenever possible for higher quality and smaller file size
2. **Specify dimensions** when embedding SVG in HTML to maintain proper proportions
3. **Respect the clear space** around the logo (minimum equal to "E" height)
4. **Use the appropriate version** (color or reversed) based on background color
5. **Don't rasterize** SVG files unnecessarily - use them directly
6. **Don't modify** the logo colors or proportions
7. When embedding inline, **ensure IDs are unique** to avoid conflicts

## SVG Embedding Methods

### HTML Image Tag
```html
<img src="path/to/edgex_full_color.svg" alt="EdgeX Logo" width="300" height="100">
```

### CSS Background
```css
.logo {
  background-image: url('path/to/edgex_full_color.svg');
  background-size: contain;
  background-repeat: no-repeat;
  width: 300px;
  height: 100px;
}
```

### Inline SVG (for advanced manipulation)
```html
<!-- Copy the content of the SVG file here -->
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 300 100">...</svg>
```

## Available SVG Files

### Full Logo (Symbol + Wordmark)
- `full/edgex_full_color.svg` - Standard full logo for light backgrounds
- `full/edgex_full_reversed.svg` - Reversed full logo for dark backgrounds

### Symbol Only
- `symbol/edgex_symbol_color.svg` - Hexagonal network symbol in standard colors
- `symbol/edgex_symbol_reversed.svg` - Hexagonal network symbol in white for dark backgrounds

### Wordmark Only
- `wordmark/edgex_wordmark_color.svg` - "EdgeX" text logo in Deep Quantum Blue
- `wordmark/edgex_wordmark_reversed.svg` - "EdgeX" text logo in white for dark backgrounds

## Optimization

All SVG files in this directory have been optimized to:
- Remove unnecessary metadata and editor data
- Simplify paths where possible without visual degradation
- Ensure proper namespace declarations
- Include appropriate metadata for accessibility
- Use consistent naming of elements and attributes

## Questions and Support

For questions about SVG logo usage or to request modifications, please contact the EdgeX marketing team or open an issue in the repository.

