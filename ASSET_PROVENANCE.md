# Asset provenance and public package scope

The private development snapshot contained complete editor projects, original comparison pixels, final images, and generated material. This repository starts a separate history and does not import those historical blobs.

## Published representation

- Native port source and conversion scripts.
- Original input filenames, sizes, and cryptographic hashes.
- ROM-relative image recipes: source map addresses, sprite/FIX indices, composition/scale parameters, source and output hashes, and sparse changed pixel positions/values.
- Editor structure and formatting with full pixel rows replaced by image references, plus palette/placement metadata.
- Small textual helpers and ranking configuration needed by the build.

`assets/index.json` describes 7,397 unique image planes used to reconstruct 155 local inputs. Across 6,324,604 pixels in those planes, 217,072 changed positions are stored. 1,497 planes have no pixel changes and resolve entirely from their source. These are storage statistics, not an authorship or legal assessment.

The source for an image is a validated original ROM-derived image or a previously reconstructed image whose dependency chain starts at the original ROM. Full original or edited image arrays, screenshots, music, compiled cartridges, and BIOS files are not included. The build reconstructs the legacy input formats locally to preserve the existing output, including comparison images used by editing tools.

## Rights distinction

Bangoo's independently drawn elements are distinct from edits based on original characters, fonts, and other original artwork. Redrawing, resizing, changing pixels, or storing a patch does not by itself establish exclusive ownership or remove underlying rights. The recipes preserve development work while excluding complete asset files from this distribution; they are not a claim that every patch is legally independent.

Do not replace these recipes with full generated `art/` contents in a public commit. The same exclusion applies to release attachments, CI artifacts, and issue uploads. See NOTICE.md and THIRD_PARTY_NOTICES.md.
