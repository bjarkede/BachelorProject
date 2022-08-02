--[[
Copyright (c) 2021-2022 Bjarke Damsgaard Eriksen. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    1. Redistributions of source code must retain the above
       copyright notice, this list of conditions and the
       following disclaimer.

    2. Redistributions in binary form must reproduce the above
       copyright notice, this list of conditions and the following
       disclaimer in the documentation and/or other materials
       provided with the distribution.

    3. Neither the name of the copyright holder nor the names of
       its contributors may be used to endorse or promote products
       derived from this software without specific prior written
       permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
--]]

local function SetProjectOptions()
	language "C++"
	configuration "Debug"
		targetdir "../../bin_debug"
		debugdir "$(BJARKE_DEBUG_PATH)/bin_debug"
		debugcmd "$(BJARKE_DEBUG_PATH)/bin_debug/$(TargetName).exe"

	configuration "Release"
		targetdir "../../bin_release"
		debugdir "$(BJARKE_DEBUG_PATH)/bin_release"
		debugcmd "$(BJARKE_DEBUG_PATH)/bin_release/$(TargetName).exe"

	configuration {}
	
	flags { "StaticRuntime", "Symbols" }
	buildoptions
	{
		"/nologo",
		"/wd4244", -- possible loss of data (general)
		"/wd4267"  -- possible loss of data (from size_t)
	}
	defines
	{
		"_CRT_SECURE_NO_WARNINGS=1", -- MS' libc string function replacements
		"_CRT_NONSTDC_NO_WARNINGS=1" -- POSIX deprecation
	}

	configuration "Debug"
		defines { "_DEBUG=1", "DEBUG=1" }
		buildoptions { "/Ob0" }
	configuration { "Debug", "vs2013" }
		flags { "EnableMinimalRebuild" } -- deprecated in vs2019, not sure since when

	configuration "Release"
		buildoptions { "/Ob2", "/GL" }
		flags { "FloatFast", "OptimizeSpeed", "NoIncrementalLink" }
		linkoptions { "/LTCG" }

	configuration {}
end

local function AddSourceFolders(rootPath, folders)
	for i, folder in ipairs(folders) do
		files { string.format("%s/%s/*.cpp", rootPath, folder), string.format("%s/%s/*.h", rootPath, folder) }
	end
end

-- type: "vs" becomes "vs_5_0"
local function AddShader(inputFile, outputFile, type, variableName, definesTable)
	definesTable = definesTable or {}; -- make the default value an empty table
	local defines = table.implode(definesTable, "/D", " ", ""); -- order: before after between

	local dirPath = "../code/shaders";
	local inputPath = string.format("%s/%s", dirPath, inputFile);
	local outputPath = string.format("%s/%s", dirPath, outputFile);

	local optionsGeneral = "/nologo";
	local optionsDebug = "/Od /Zi";
	local optionsRelease = "/O3";

	local formatString = "fxc %s %s %s /T %s_5_0 /E %s_main /Vn %s /Fh $(@) $(<)";
	local commandDebug = string.format(formatString, defines, optionsGeneral, optionsDebug, type, type, variableName);
	local commandRelease = string.format(formatString, defines, optionsGeneral, optionsRelease, type, type, variableName);
	local commandAll = string.format("if $(ConfigurationName) == Debug ( %s ) else ( %s )", commandDebug, commandRelease);

	custombuildtask
	{
		{
			inputPath,
			outputPath,
			{ "fxc" },
			{ commandAll }
		}
	}
end

solution "Bachelor"
	location(_ACTION)
	configurations { "Debug", "Release" }
	platforms { "x64" }
	configuration "windows"

	project "Bachelor"
		kind "WindowedApp"
		SetProjectOptions()					
		links { "user32", "d3d11", "dxguid" }	
		defines { "IMGUI_IMPL_WIN32_DISABLE_GAMEPAD" }
		flags { "NoWinRT", "WinMain" }
		
		AddSourceFolders("../code", { "common", "ddx-kts", "imgui", "renderer", "scene", "win32" })
		files { "../code/shaders/*.hlsli" }
		files { "premake4.lua" }
		files { "txt/*.txt" }

		AddShader("dear_imgui.hlsl", "dear_imgui_vs.h", "vs", "g_vs")
		AddShader("dear_imgui.hlsl", "dear_imgui_ps.h", "ps", "g_ps")

		AddShader("debug_viz.hlsl", "debug_viz_vs.h", "vs", "g_vs")
		AddShader("debug_viz.hlsl", "debug_viz_ps.h", "ps", "g_ps")

		AddShader("opacity_voxelization.hlsl", "opacity_voxelization_vs.h", "vs", "g_vs")
		AddShader("opacity_voxelization.hlsl", "opacity_voxelization_cr1_gs.h", "gs", "g_gs_cr1", { "CONS_RASTER=1" })
		AddShader("opacity_voxelization.hlsl", "opacity_voxelization_cr0_gs.h", "gs", "g_gs_cr0")
		AddShader("opacity_voxelization.hlsl", "opacity_voxelization_cr1_ps.h", "ps", "g_ps_cr1", { "CONS_RASTER=1" })
		AddShader("opacity_voxelization.hlsl", "opacity_voxelization_cr0_ps.h", "ps", "g_ps_cr0")

		AddShader("emittance_voxelization.hlsl", "emittance_voxelization_vs.h", "vs", "g_vs")
		AddShader("emittance_voxelization.hlsl", "emittance_voxelization_cr1_gs.h", "gs", "g_gs_cr1", { "CONS_RASTER=1" })
		AddShader("emittance_voxelization.hlsl", "emittance_voxelization_cr0_gs.h", "gs", "g_gs_cr0")
		AddShader("emittance_voxelization.hlsl", "emittance_voxelization_cr1_ps.h", "ps", "g_ps_cr1", { "CONS_RASTER=1"})
		AddShader("emittance_voxelization.hlsl", "emittance_voxelization_cr0_ps.h", "ps", "g_ps_cr0")

		AddShader("voxel_vis.hlsl", "voxel_vis_vs.h", "vs", "g_vs")
		AddShader("voxel_vis.hlsl", "voxel_vis_gs.h", "gs", "g_gs")
		AddShader("voxel_vis.hlsl", "voxel_vis_ps.h", "ps", "g_ps")

		AddShader("geometry_pass.hlsl", "geometry_pass_vs.h", "vs", "g_vs")
		AddShader("geometry_pass.hlsl", "geometry_pass_ps.h", "ps", "g_ps")

		AddShader("deferred_shading.hlsl", "deferred_shading_vs.h", "vs", "g_vs")
		AddShader("deferred_shading.hlsl", "deferred_shading_ps.h", "ps", "g_ps")

		AddShader("shadows.hlsl", "shadows_vs.h", "vs", "g_vs")
		AddShader("shadows.hlsl", "shadows_gs.h", "gs", "g_gs")
		AddShader("shadows.hlsl", "shadows_ps.h", "ps", "g_ps")

		AddShader("opacity_mip_downsample.hlsl", "opacity_mip_downsample_cs.h", "cs", "g_cs")
		AddShader("emittance_mip_downsample.hlsl", "emittance_mip_downsample_cs.h", "cs", "g_cs")
		AddShader("emittance_format_fix.hlsl", "emittance_format_fix_cs.h", "cs", "g_cs")
		AddShader("emittance_opacity_fix.hlsl", "emittance_opacity_fix_cs.h", "cs", "g_cs")
		AddShader("emittance_bounce.hlsl", "emittance_bounce_cs.h", "cs", "g_cs")

		AddShader("voxelization_fix.hlsl", "voxelization_fix_cs.h", "cs", "g_cs")

		AddShader("hiz.hlsl", "hiz_cs.h", "cs", "g_cs");

		AddShader("ssso.hlsl", "ssso_ps.h", "ps", "g_ps");
		AddShader("ssso.hlsl", "ssso_vs.h", "vs", "g_vs");

		AddShader("ssso_filter.hlsl", "ssso_filter_vs.h", "vs", "g_vs");
		AddShader("ssso_filter.hlsl", "ssso_filter_ps.h", "ps", "g_ps");

	project "Hemisphere"
		kind "ConsoleApp"
		SetProjectOptions()
			
		files { "../code/tools/hemisphere/*.h", "../code/tools/hemisphere/*.cpp" }

	project "MeshBaker"
		kind "ConsoleApp"
		SetProjectOptions()

		files { "../code/tools/mesh_baker/*.h", "../code/tools/mesh_baker/*.cpp", "../code/common/parsing.cpp", "../code/win32/win32_api.cpp", "../code/common/shared.cpp"}
		--AddSourceFolders("../code", { "common", "ddx-kts", "imgui", "scene", "win32" })