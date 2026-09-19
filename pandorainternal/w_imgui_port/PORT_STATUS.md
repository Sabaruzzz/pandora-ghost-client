# W ImGui port status

This directory is an untouched copy of the supplied W renderer/framework plus a reconstructed project umbrella.
It is intentionally not part of `pandorainternal.vcxproj` yet, so the verified pandora build remains recoverable.

Completed:
- exact W source snapshot copied;
- missing `includes.hh` reconstructed;
- missing `math::c_vector_2d` and `hue::c_color` isolated in `compatibility.hh`;
- original W animation, child, slider, checkbox, dropdown, color picker, search and window implementations preserved.

Next integration gate:
- compile W as a separate translation-unit set;
- fill any omitted renderer/platform contracts revealed by compilation;
- connect a pandora module adapter without rewriting W widgets;
- switch renderers only after behavior parity checks.
