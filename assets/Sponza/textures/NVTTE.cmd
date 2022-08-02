@echo off

set PATH=C:\Programs\NVIDIA Texture Tools Exporter;%PATH%
set shared=--quality 1 --mips --mip-filter 1 --no-mip-pre-alpha
set albedo=--format 23 --mip-gamma-correct
set specular=--format 20 --no-mip-gamma-correct --to-grayscale
set make_normal=--format 21 --no-mip-gamma-correct --to-normal-ts --normal-filter 5 --normal-scale 12 --normalize --normal-invert-y --wrap
set normal=--format 21 --no-mip-gamma-correct
set alpha=--format 23 --mip-gamma-correct --alpha-treshold 127 --cutout-alpha --scale-alpha --cutout-alpha-dither --save-flip-y

@echo on

for %%f in (*_diff.png) do (
	nvtt_export.exe --output-file %%~nf.dds %shared% %albedo% %%f || goto :done
)

for %%f in (*_spec.png) do (
	nvtt_export.exe --output-file %%~nf.dds %shared% %specular% %%f || goto :done
)

goto :ddn
:: generates normap maps from height maps
for %%f in (*_bump.png) do (
	nvtt_export.exe --output-file %%~nf.dds %shared% %make_normal% %%f || goto :done
)
:ddn
:: encodes existing tangent-space normal maps
for %%f in (*_ddn.png) do (
	nvtt_export.exe --output-file %%~nf.dds %shared% %normal% %%f || goto :done
)

:: for whatever reason this doesn't work from the command-line...
nvtt_export.exe --output-file chain_texture_diff.dds %shared% %albedo% chain_texture_diff.png || goto :done
nvtt_export.exe --output-file sponza_thorn_diff.dds %shared% %albedo% sponza_thorn_diff.png || goto :done
nvtt_export.exe --output-file vase_plant_diff.dds %shared% %albedo% vase_plant_diff.png || goto :done

:done
pause
exit

could use these normal maps (they have scratch marks) as-is...
...but they're very aliased:
background_ddn.png
lion_ddn.png

--quality
0=fastest
1=normal
2=production
3=highest

albedo   : BC7
specular : BC4u
normal   : BC5u
