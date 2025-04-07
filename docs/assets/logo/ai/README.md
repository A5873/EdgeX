# EdgeX Adobe Illustrator Master Files

This directory contains the master Adobe Illustrator (.ai) files for the EdgeX logo system. These files serve as the definitive source for all logo variants and should be used when professional editing is required.

## Purpose of AI Master Files

The Adobe Illustrator files are the original source files used to create all EdgeX logo assets. They provide:

- **Master Source**: Definitive versions with full editability
- **High Precision**: Pixel-perfect vector artwork with precise measurements
- **Design Flexibility**: Complete toolset for professional modifications
- **Export Control**: Source for generating all other format variations
- **Design Consistency**: Contains all brand elements in a controlled environment
- **Typography Management**: Proper font handling for the wordmark

## File Organization

### Master Files

- `edgex_logo_master.ai` - Complete logo system with all components and variants
- `edgex_symbol_master.ai` - Symbol component isolated for detailed editing
- `edgex_wordmark_master.ai` - Wordmark component isolated for typography work

### Artboards Structure

Each master file contains multiple artboards:

1. **Color Variants** - Standard color versions on light background
2. **Reversed Variants** - White/reversed versions for dark backgrounds
3. **Construction** - Guides and measurements showing proper construction
4. **Clear Space** - Demonstrations of minimum clear space requirements
5. **Size Variants** - Examples of logo at various sizes for quality checking

## Layer Organization

All files follow a consistent layer structure:

- **Guides** - Non-printing guides for alignment and proportions
- **Logo Components** - Separated elements of the logo
  - **Symbol** - The hexagonal network symbol
    - Structure (Deep Quantum Blue)
    - Nodes (Edge Teal)
  - **Wordmark** - The "EdgeX" text
- **Background** - Toggle-able background for testing contrast
- **Metadata** - Copyright and version information
- **Export Artboards** - Pre-configured artboards for standard exports

## Usage Guidelines for Designers

1. **Always work from master files** - Never recreate elements from scratch
2. **Preserve artboard dimensions** - Maintain exact sizing and proportions
3. **Use linked components** where possible to maintain consistency
4. **Never rasterize** vector components
5. **Preserve layer structure** when adding variations
6. **Document any modifications** in the version history layer
7. **Save iterative versions** rather than overwriting files
8. **Check alignment** against grid and guides before finalizing

## Color Swatches and Typography

### Brand Colors (Included in Swatch Library)

| Color Name | RGB | CMYK | HEX | Pantone |
|------------|-----|------|-----|---------|
| Deep Quantum Blue | 11,36,71 | 100,85,44,47 | #0B2447 | PMS 534 C |
| Edge Teal | 25,167,206 | 70,10,10,0 | #19A7CE | PMS 313 C |
| Quantum White | 248,250,252 | 1,0,0,0 | #F8FAFC | White |

### Typography Specifications

The EdgeX wordmark uses a modified geometric sans-serif typeface:

- **Base Font**: Montserrat Bold
- **Tracking**: -15
- **Custom Modifications**:
  - Modified 'E' with extended middle arm
  - Custom 'X' with extended terminal
  - Adjusted spacing between 'g' and 'e'

All text has been converted to outlines in the final files, but original typography layers are preserved in a hidden "Typography Source" layer for reference.

## Modification Protocols

When modifications to the master files are required:

1. **Create a working copy** - Never edit the original master directly
2. **Document changes** in the version history layer
3. **Maintain proportional consistency** using the provided grid system
4. **Check modifications** against the design guidelines
5. **Test reversed versions** when making changes to ensure they work on dark backgrounds
6. **Review at multiple sizes** to ensure scalability
7. **Get approval** from the EdgeX branding team before finalizing changes
8. **Update all derived formats** (SVG, PNG) after approval

### Approved Modification Scenarios

- Adapting the logo for specific size constraints
- Creating special variants for approved marketing campaigns
- Addressing technical issues in rendering or display
- Implementing official brand updates

## Export Guidelines

### SVG Export Settings

To generate SVG files from these master AI files:

1. Use "File > Export > Export As... > SVG"
2. Select the appropriate artboard
3. Use these settings:
   - **Styling**: Presentation Attributes
   - **Font**: Convert to Outlines
   - **Images**: Preserve
   - **Object IDs**: Layer Names
   - **Decimals**: 3
   - **Minify**: No
   - **Responsive**: Yes

### PNG Export Settings

To generate PNG files:

1. Use "File > Export > Export for Screens"
2. Select the desired artboards
3. Use these settings:
   - **Scale**: 1x (or as needed)
   - **Format**: PNG-24
   - **Transparency**: Yes
   - **Suffix**: Include size in filename (e.g., _300)
   - **Color Space**: RGB

Refer to the [PNG Generation Guide](../PNG_GENERATION.md) for specific size requirements.

## Version History

| Version | Date | Designer | Changes |
|---------|------|----------|---------|
| 1.0 | 2025-04-07 | EdgeX Design Team | Initial creation of master files |

## Support and Contact

For questions about the Adobe Illustrator files or to request access to the original design files, please contact the EdgeX marketing team or open an issue in the repository.

