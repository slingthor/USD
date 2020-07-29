#!/pxrpythonsubst
#
# Copyright 2018 Pixar
#
# Licensed under the Apache License, Version 2.0 (the "Apache License")
# with the following modification; you may not use this file except in
# compliance with the Apache License and the following modification to it:
# Section 6. Trademarks. is deleted and replaced with:
#
# 6. Trademarks. This License does not grant permission to use the trade
#    names, trademarks, service marks, or product names of the Licensor
#    and its affiliates, except as required to comply with Section 4(c) of
#    the License and to reproduce the content of the NOTICE file.
#
# You may obtain a copy of the Apache License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the Apache License with the above modification is
# distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the Apache License for the specific
# language governing permissions and limitations under the Apache License.
#
#
# When run, this script will generate a series of usda files in the current
# directory that illustrate each of the steps in the tutorial.
#

from pxr import Gf, Kind, Sdf, Usd, UsdGeom, UsdShade
import os


def create_obj(stage, name, p1, p2, p3, p4, file):
    filename, file_extension = os.path.splitext(file)
    # We put both geometry and materials under a common "model root prim",
    # which makes it safe to reference the model into another scene.
    modelRoot = UsdGeom.Xform.Define(stage, name)
    Usd.ModelAPI(modelRoot).SetKind(Kind.Tokens.component)
    # A simple card with same proportions as the texture we will map
    billboard = UsdGeom.Mesh.Define(stage, name + "/Geom")
    billboard.CreatePointsAttr([p1, p2, p3, p4])
    billboard.CreateFaceVertexCountsAttr([4])
    billboard.CreateFaceVertexIndicesAttr([0, 1, 2, 3])
    billboard.CreateExtentAttr([p1, p3])
    texCoords = billboard.CreatePrimvar("st",
                                        Sdf.ValueTypeNames.TexCoord2fArray,
                                        UsdGeom.Tokens.varying)
    texCoords.Set([(0, 0), (1, 0), (1, 1), (0, 1)])

    # Now make a Material that contains a PBR preview surface, a texture reader,
    # and a primvar reader to fetch the texture coordinate from the geometry
    material = UsdShade.Material.Define(stage, name + '/Geom/Material')

    # Create surface, and connect the Material's surface output to the surface
    # shader.  Make the surface non-metallic, and somewhat rough, so it doesn't
    # glare in usdview's simple camera light setup.
    pbrShader = UsdShade.Shader.Define(
        stage, name + '/Geom/Material/SurfaceShader')
    pbrShader.CreateIdAttr("UsdPreviewSurface")
    pbrShader.CreateInput("roughness", Sdf.ValueTypeNames.Float).Set(1.0)
    pbrShader.CreateInput("metallic", Sdf.ValueTypeNames.Float).Set(0.0)

    material.CreateSurfaceOutput().ConnectToSource(pbrShader, "surface")
    # create texture coordinate reader
    stReader = UsdShade.Shader.Define(stage, name + '/Geom/Material/stReader')
    stReader.CreateIdAttr('UsdPrimvarReader_float2')

    stInput = material.CreateInput(
        'frame:stPrimvarName', Sdf.ValueTypeNames.Token)
    stInput.Set('st')

    # Note here we are connecting the shader's input to the material's
    # "public interface" attribute. This allows users to change the primvar name
    # on the material itself without drilling inside to examine shader nodes.
    stReader.CreateInput(
        'varname', Sdf.ValueTypeNames.Token).ConnectToSource(stInput)

    # diffuseAlpha texture
    diffuseAlphaTextureSampler = UsdShade.Shader.Define(
        stage, name + '/Geom/Material/DiffuseAlphaTexture')
    diffuseAlphaTextureSampler.CreateIdAttr('UsdUVTexture')
    diffuseAlphaTextureSampler.CreateInput(
        'file', Sdf.ValueTypeNames.Asset).Set(file)
    diffuseAlphaTextureSampler.CreateInput(
        "st", Sdf.ValueTypeNames.Float2).ConnectToSource(stReader, 'result')
    diffuseAlphaTextureSampler.CreateOutput('rgb', Sdf.ValueTypeNames.Float3)
    diffuseAlphaTextureSampler.CreateOutput('a', Sdf.ValueTypeNames.Float)
    pbrShader.CreateInput("diffuseColor", Sdf.ValueTypeNames.Color3f).ConnectToSource(
        diffuseAlphaTextureSampler, 'rgb')
    pbrShader.CreateInput("opacity", Sdf.ValueTypeNames.Float).ConnectToSource(
        diffuseAlphaTextureSampler, 'a')

    # Now bind the Material to the card
    UsdShade.MaterialBindingAPI(billboard).Bind(material)


def main():
    stage0 = Usd.Stage.CreateNew("pngUse.usda")
    UsdGeom.SetStageUpAxis(stage0, UsdGeom.Tokens.y)
    create_obj(stage0, "/PNG_ALPHA", (-430, -160, 0), (430, -160, 0),
               (430, 160, 0), (-430, 160, 0), "0/Testcircleerased.png")
    stage0.Save()

    stage2 = Usd.Stage.CreateNew("basisUse.usda")
    UsdGeom.SetStageUpAxis(stage2, UsdGeom.Tokens.y)
    create_obj(stage2, "/BASIS_ALPHA", (-430, -160, 0), (430, -160, 0),
               (430, 160, 0), (-430, 160, 0), "0/Testcircleerased.basis")
    stage2.Save()


if __name__ == "__main__":
    main()
