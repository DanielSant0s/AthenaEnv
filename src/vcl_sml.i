;//--------------------------------------------------------------------------------
;// VCLSML - VCL Standard Macros Library
;//
;// Geoff Audy, January 17th 2002  Initial macro set
;// Geoff Audy, March 26th 2002    Fixed some macros that were broken
;// Geoff Audy, March 27th 2002    Added some macros, most from Colin Hugues (SCEE)
;// Linux (for PlayStation 2) release version 1.10 July 2002
;//
;// Copyright (C) 2002, Sony Computer Entertainment America Inc.
;// All rights reserved.
;//
;// Note: Some macros generate the following temporary variables:
;//           vclsmlftemp:   Temporary float register
;//           vclsmlitemp:   Temporary integer register
;//--------------------------------------------------------------------------------

;//--------------------------------------------------------------------
;// MatrixLoad - Load "matrix" from VU mem location "vumemlocation" +
;// "offset"
;//--------------------------------------------------------------------
   .macro   MatrixLoad  matrix,offset,vumemlocation
   lq             \matrix[0], \offset+0(\vumemlocation)
   lq             \matrix[1], \offset+1(\vumemlocation)
   lq             \matrix[2], \offset+2(\vumemlocation)
   lq             \matrix[3], \offset+3(\vumemlocation)
   .endm

;//--------------------------------------------------------------------
;// MatrixSave - Save "matrix" to VU mem location "vumemlocation" +
;// "offset"
;//--------------------------------------------------------------------
   .macro   MatrixSave  matrix,offset,vumemlocation
   sq             \matrix[0], \offset+0(\vumemlocation)
   sq             \matrix[1], \offset+1(\vumemlocation)
   sq             \matrix[2], \offset+2(\vumemlocation)
   sq             \matrix[3], \offset+3(\vumemlocation)
   .endm

;//--------------------------------------------------------------------
;// MatrixIdentity - Set "matrix" to be an identity matrix
;// Thanks to Colin Hugues (SCEE) for that one
;//--------------------------------------------------------------------
   .macro   MatrixIdentity matrix
   add.x          \matrix[0], vf00, vf00[w]
   mfir.yzw       \matrix[0], vi00

   mfir.xzw       \matrix[1], vi00
   add.y          \matrix[1], vf00, vf00[w]

   mr32           \matrix[2], vf00

   max            \matrix[3], vf00, vf00
   .endm

;//--------------------------------------------------------------------
;// MatrixCopy - Copy "matrixsrc" to "matrixdest"
;// Thanks to Colin Hugues (SCEE) for that one
;//--------------------------------------------------------------------
   .macro   MatrixCopy matrixdest,matrixsrc
   max            \matrixdest[0], \matrixsrc[0], \matrixsrc[0]
   move           \matrixdest[1], \matrixsrc[1]
   max            \matrixdest[2], \matrixsrc[2], \matrixsrc[2]
   move           \matrixdest[3], \matrixsrc[3]
   .endm

;//--------------------------------------------------------------------
;// MatrixSwap - Swap the content of "matrix1" and "matrix2"
;// The implementation seems lame, but VCL will convert moves to maxes
;// if it sees fit
;//--------------------------------------------------------------------
   .macro   MatrixSwap matrix1,matrix2
   move           vclsmlftemp, \matrix1[0]
   move           \matrix1[0], \matrix2[0]
   move           \matrix2[0], vclsmlftemp

   move           vclsmlftemp, \matrix1[1]
   move           \matrix1[1], \matrix2[1]
   move           \matrix2[1], vclsmlftemp

   move           vclsmlftemp, \matrix1[2]
   move           \matrix1[2], \matrix2[2]
   move           \matrix2[2], vclsmlftemp

   move           vclsmlftemp, \matrix1[3]
   move           \matrix1[3], \matrix2[3]
   move           \matrix2[3], vclsmlftemp
   .endm

;//--------------------------------------------------------------------
;// MatrixTranspose - Transpose "matrixsrc" to "matresult".  It is safe
;// for "matrixsrc" and "matresult" to be the same.
;// Thanks to Colin Hugues (SCEE) for that one
;// Had to modify it though, it was too... smart.
;//--------------------------------------------------------------------
   .macro   MatrixTranspose matresult,matrixsrc
   mr32.y         vclsmlftemp,   \matrixsrc[1]
   add.z          \matresult[1], vf00, \matrixsrc[2][y]
   move.y         \matresult[2], vclsmlftemp
   mr32.y         vclsmlftemp,   \matrixsrc[0]
   add.z          \matresult[0], vf00, \matrixsrc[2][x]
   mr32.z         vclsmlftemp,   \matrixsrc[1]
   mul.w          \matresult[1], vf00, \matrixsrc[3][y]
   mr32.x         vclsmlftemp,   \matrixsrc[0]
   add.y          \matresult[0], vf00, \matrixsrc[1][x]
   move.x         \matresult[1], vclsmlftemp
   mul.w          vclsmlftemp,   vf00, \matrixsrc[3][z]
   mr32.z         \matresult[3], \matrixsrc[2]
   move.w         \matresult[2], vclsmlftemp
   mr32.w         vclsmlftemp,   \matrixsrc[3]
   add.x          \matresult[3], vf00, \matrixsrc[0][w]
   move.w         \matresult[0], vclsmlftemp
   mr32.y         \matresult[3], vclsmlftemp
   add.x          \matresult[2], vf00, vclsmlftemp[y]

   move.x         \matresult[0], \matrixsrc[0]              ;// These 4 instructions will be
   move.y         \matresult[1], \matrixsrc[1]              ;// removed if "matrixsrc" and
   move.z         \matresult[2], \matrixsrc[2]              ;// "matresult" are the same
   move.w         \matresult[3], \matrixsrc[3]              ;//
   .endm

;//--------------------------------------------------------------------
;// MatrixMultiply - Multiply 2 matrices, "matleft" and "matright", and
;// output the result in "matresult".  Dont forget matrix multipli-
;// cations arent commutative, i.e. left X right wont give you the
;// same result as right X left.
;//
;// Note: ACC register is modified
;//--------------------------------------------------------------------
   .macro   MatrixMultiply   matresult,matleft,matright
   mul            acc,           \matright[0], \matleft[0][x]
   madd           acc,           \matright[1], \matleft[0][y]
   madd           acc,           \matright[2], \matleft[0][z]
   madd           \matresult[0], \matright[3], \matleft[0][w]

   mul            acc,           \matright[0], \matleft[1][x]
   madd           acc,           \matright[1], \matleft[1][y]
   madd           acc,           \matright[2], \matleft[1][z]
   madd           \matresult[1], \matright[3], \matleft[1][w]

   mul            acc,           \matright[0], \matleft[2][x]
   madd           acc,           \matright[1], \matleft[2][y]
   madd           acc,           \matright[2], \matleft[2][z]
   madd           \matresult[2], \matright[3], \matleft[2][w]

   mul            acc,           \matright[0], \matleft[3][x]
   madd           acc,           \matright[1], \matleft[3][y]
   madd           acc,           \matright[2], \matleft[3][z]
   madd           \matresult[3], \matright[3], \matleft[3][w]
   .endm

;//--------------------------------------------------------------------
;// LocalizeLightMatrix - Transform the light matrix "lightmatrix" into
;// local space, as described by "matrix", and output the result in
;// "locallightmatrix"
;//
;// Note: ACC register is modified
;//--------------------------------------------------------------------
   .macro   LocalizeLightMatrix locallightmatrix,matrix,lightmatrix
   mul            acc,                  \lightmatrix[0], \matrix[0][x]
   madd           acc,                  \lightmatrix[1], \matrix[0][y]
   madd           acc,                  \lightmatrix[2], \matrix[0][z]
   madd           \locallightmatrix[0], \lightmatrix[3], \matrix[0][w]

   mul            acc,                  \lightmatrix[0], \matrix[1][x]
   madd           acc,                  \lightmatrix[1], \matrix[1][y]
   madd           acc,                  \lightmatrix[2], \matrix[1][z]
   madd           \locallightmatrix[1], \lightmatrix[3], \matrix[1][w]

   mul            acc,                  \lightmatrix[0], \matrix[2][x]
   madd           acc,                  \lightmatrix[1], \matrix[2][y]
   madd           acc,                  \lightmatrix[2], \matrix[2][z]
   madd           \locallightmatrix[2], \lightmatrix[3], \matrix[2][w]

   move           \locallightmatrix[3], \lightmatrix[3]
   .endm

;//--------------------------------------------------------------------
;// MatrixMultiplyVertex - Multiply "matrix" by "vertex", and output
;// the result in "vertexresult"
;//
;// Note: Apply rotation, scale and translation
;// Note: ACC register is modified
;//--------------------------------------------------------------------
   .macro   MatrixMultiplyVertex vertexresult,matrix,vertex
   mul            acc,           \matrix[0], \vertex[x]
   madd           acc,           \matrix[1], \vertex[y]
   madd           acc,           \matrix[2], \vertex[z]
   madd           \vertexresult, \matrix[3], \vertex[w]
   .endm

;//--------------------------------------------------------------------
;// MatrixMultiplyVertex - Multiply "matrix" by "vertex", and output
;// the result in "vertexresult"
;//
;// Note: Apply rotation, scale and translation
;// Note: ACC register is modified
;//--------------------------------------------------------------------
   .macro   MatrixMultiplyVertexXYZ1 vertexresult,matrix,vertex
   mul            acc,           \matrix[0], \vertex[x]
   madd           acc,           \matrix[1], \vertex[y]
   madd           acc,           \matrix[2], \vertex[z]
   madd           \vertexresult, \matrix[3], vf00[w]
   .endm

;//--------------------------------------------------------------------
;// MatrixMultiplyVector - Multiply "matrix" by "vector", and output
;// the result in "vectorresult"
;//
;// Note: Apply rotation and scale, but no translation
;// Note: ACC register is modified
;//--------------------------------------------------------------------
   .macro   MatrixMultiplyVector vectorresult,matrix,vector
   mul            acc,           \matrix[0], \vector[x]
   madd           acc,           \matrix[1], \vector[y]
   madd           \vectorresult, \matrix[2], \vector[z]
   .endm

;//--------------------------------------------------------------------
;// VectorLoad - Load "vector" from VU mem location "vumemlocation" +
;// "offset"
;//--------------------------------------------------------------------
   .macro   VectorLoad  vector,offset,vumemlocation
   lq             \vector, \offset(\vumemlocation)
   .endm

;//--------------------------------------------------------------------
;// VectorSave - Save "vector" to VU mem location "vumemlocation" +
;// "offset"
;//--------------------------------------------------------------------
   .macro   VectorSave  vector,offset,vumemlocation
   sq             \vector, \offset(\vumemlocation)
   .endm

;//--------------------------------------------------------------------
;// VectorAdd - Add 2 vectors, "vector1" and "vector2" and output the
;// result in "vectorresult"
;//--------------------------------------------------------------------
   .macro   VectorAdd   vectorresult,vector1,vector2
   add            \vectorresult, \vector1, \vector2
   .endm

;//--------------------------------------------------------------------
;// VectorSub - Subtract "vector2" from "vector1", and output the
;// result in "vectorresult"
;//--------------------------------------------------------------------
   .macro   VectorSub   vectorresult,vector1,vector2
   sub            \vectorresult, \vector1, \vector2
   .endm

;//--------------------------------------------------------------------
;// VertexLoad - Load "vertex" from VU mem location "vumemlocation" +
;// "offset"
;//--------------------------------------------------------------------
   .macro   VertexLoad  vertex,offset,vumemlocation
   lq             \vertex, \offset(\vumemlocation)
   .endm

;//--------------------------------------------------------------------
;// VertexSave - Save "vertex" to VU mem location "vumemlocation" +
;// "offset"
;//--------------------------------------------------------------------
   .macro   VertexSave  vertex,offset,vumemlocation
   sq             \vertex, \offset(\vumemlocation)
   .endm

;//--------------------------------------------------------------------
;// VertexPersCorr - Apply perspective correction onto "vertex" and
;// output the result in "vertexoutput"
;//
;// Note: Q register is modified
;//--------------------------------------------------------------------
   .macro   VertexPersCorr vertexoutput,vertex
   div            q, vf00[w], \vertex[w]
;   mul.xyz        \vertexoutput, \vertex, q
   mul            \vertexoutput, \vertex, q
   .endm

;//--------------------------------------------------------------------
;// VertexPersCorrST - Apply perspective correction onto "vertex" and
;// "st", and output the result in "vertexoutput" and "stoutput"
;//
;// Note: Q register is modified
;//--------------------------------------------------------------------
   .macro   VertexPersCorrST vertexoutput,stoutput,vertex,st
   div            q,             vf00[w], \vertex[w]
   mul.xyz        \vertexoutput, \vertex, q
   move.w         \vertexoutput, \vertex
;   mul            \vertexoutput, \vertex, q
   mul            \stoutput,     \st,     q
   .endm

;//--------------------------------------------------------------------
;// VertexFPtoGsXYZ2 - Convert an XYZW, floating-point vertex to GS
;// XYZ2 format (ADC bit isnt set)
;//--------------------------------------------------------------------
   .macro   VertexFpToGsXYZ2  outputxyz,vertex
   ftoi4.xy       \outputxyz, \vertex
   ftoi0.z        \outputxyz, \vertex
   mfir.w         \outputxyz, vi00
   .endm

;//--------------------------------------------------------------------
;// VertexFPtoGsXYZ2Adc - Convert an XYZW, floating-point vertex to GS
;// XYZ2 format (ADC bit is set)
;//--------------------------------------------------------------------
   .macro   VertexFpToGsXYZ2Adc  outputxyz,vertex
   ftoi4.xy       \outputxyz, \vertex
   ftoi0.z        \outputxyz, \vertex
   ftoi15.w       \outputxyz, vf00
   .endm

;//--------------------------------------------------------------------
;// VertexFpToGsXYZF2 - Convert an XYZF, floating-point vertex to GS
;// XYZF2 format (ADC bit isnt set)
;//--------------------------------------------------------------------
   .macro   VertexFpToGsXYZF2 outputxyz,vertex
   ftoi4          \outputxyz, \vertex
   .endm

;//--------------------------------------------------------------------
;// VertexFpToGsXYZF2Adc - Convert an XYZF, floating-point vertex to GS
;// XYZF2 format (ADC bit is set)
;//--------------------------------------------------------------------
   .macro   VertexFpToGsXYZF2Adc outputxyz,vertex
   ftoi4          \outputxyz,  \vertex
   mtir           vclsmlitemp, \outputxyz[w]
   iaddiu         vclsmlitemp, 0x7FFF
   iaddi          vclsmlitemp, 1
   mfir.w         \outputxyz,  vclsmlitemp
   .endm

;//--------------------------------------------------------------------
;// ColorFPtoGsRGBAQ - Convert an RGBA, floating-point color to GS
;// RGBAQ format
;//--------------------------------------------------------------------
   .macro   ColorFPtoGsRGBAQ  outputrgba,color
   ftoi0          \outputrgba, \color
   .endm

;//--------------------------------------------------------------------
;// ColorGsRGBAQtoFP - Convert an RGBA, GS RGBAQ format to floating-
;// point color
;//--------------------------------------------------------------------
   .macro   ColorGsRGBAQtoFP  outputrgba,color
   itof0          \outputrgba, \color
   .endm

;//--------------------------------------------------------------------
;// CreateGsPRIM - Create a GS-packed-format PRIM command, according to
;// a specified immediate value "prim"
;//
;// Note: Meant more for debugging purposes than for a final solution
;//--------------------------------------------------------------------
   .macro   CreateGsPRIM   outputprim,prim
   iaddiu         vclsmlitemp, vi00, \prim
   mfir           \outputprim, vclsmlitemp
   .endm

;//--------------------------------------------------------------------
;// CreateGsRGBA - Create a GS-packed-format RGBA command, according to
;// specified immediate values "r", "g", "b" and "a" (integer 0-255)
;//
;// Note: Meant more for debugging purposes than for a final solution
;//--------------------------------------------------------------------
   .macro   CreateGsRGBA   outputrgba,r,g,b,a
   iaddiu         vclsmlitemp, vi00, \r
   mfir.x         \outputrgba, vclsmlitemp
   iaddiu         vclsmlitemp, vi00, \g
   mfir.y         \outputrgba, vclsmlitemp
   iaddiu         vclsmlitemp, vi00, \b
   mfir.z         \outputrgba, vclsmlitemp
   iaddiu         vclsmlitemp, vi00, \a
   mfir.w         \outputrgba, vclsmlitemp
   .endm

;//--------------------------------------------------------------------
;// CreateGsSTQ - Create a GS-packed-format STQ command, according to
;// specified immediate values "s", "t" and "q" (floats)
;//
;// Note: I register is modified
;// Note: Meant more for debugging purposes than for a final solution
;//--------------------------------------------------------------------
   .macro   CreateGsSTQ   outputstq,s,t,q
   loi            \s
   add.x          \outputstq, vf00, i
   loi            \t
   add.y          \outputstq, vf00, i
   loi            \q
   add.z          \outputstq, vf00, i
   .endm

;//--------------------------------------------------------------------
;// CreateGsUV - Create a GS-packed-format VU command, according to
;// specified immediate values "u" and "v" (integer -32768 - 32768,
;// with 4 LSB as precision)
;//
;// Note: Meant more for debugging purposes than for a final solution
;//--------------------------------------------------------------------
   .macro   CreateGsUV   outputuv,u,v
   iaddiu         vclsmlitemp, vi00, \u
   mfir.x         \outputuv, vclsmlitemp
   iaddiu         vclsmlitemp, vi00, \v
   mfir.y         \outputuv, vclsmlitemp
   .endm

;//--------------------------------------------------------------------
;// CreateGsRGBA - Create a GS-packed-format RGBA command, according to
;// a specified immediate value "fog" (integer 0-255)
;//
;// Note: Meant more for debugging purposes than for a final solution
;//--------------------------------------------------------------------
   .macro   CreateGsFOG   outputfog,fog
   iaddiu         vclsmlitemp, vi00, \fog * 16
   mfir.w         \outputfog, vclsmlitemp
   .endm

;//--------------------------------------------------------------------
;// CreateGifTag - Create a packed-mode giftag, according to specified
;// immediate values.  Currently only support up to 4 registers.
;//
;// Note: I register is modified
;// Note: Definitely meant for debugging purposes, NOT for a final
;// solution
;//--------------------------------------------------------------------
;// MIGHT NOT BE IMPLEMENTABLE AFTER ALL (AT LEAST NOT UNTIL VCL EVALUATES CONSTANTS!)
;// THAT WOULD HAVE BEEN KINDA COOL...  DAMN.  --GEOFF
;//   .macro   CreateGifTag   outputgiftag,nloop,prim,nreg,reg1,reg2,reg3,reg4
;//   iaddiu         vclsmlitemp, vi00, \nloop + 0x8000
;//   mfir.x         \outputgiftag, vclsmlitemp
;//   loi            0x00004000 + (\prim * 0x8000) + (\nreg * 0x10000000)
;//   add.y          \outputgiftag, vf00, i
;//   iaddiu         vclsmlitemp, vi00, \reg1 + (\reg2 * 16) + (\reg3 * 256) + (\reg4 * 4096)
;//   mfir.z         \outputgiftag, vclsmlitemp
;//   .endm

;//--------------------------------------------------------------------
;// VectorDotProduct - Calculate the dot product of "vector1" and
;// "vector2", and output to "dotproduct"[x]
;//--------------------------------------------------------------------
   .macro   VectorDotProduct  dotproduct,vector1,vector2
   mul.xyz        \dotproduct, \vector1,    \vector2
   add.x          \dotproduct, \dotproduct, \dotproduct[y]
   add.x          \dotproduct, \dotproduct, \dotproduct[z]
   .endm

;//--------------------------------------------------------------------
;// VectorDotProductACC - Calculate the dot product of "vector1" and
;// "vector2", and output to "dotproduct"[x].  This one does it using
;// the ACC register which, depending on the case, might turn out to be
;// faster or slower.
;//
;// Note: ACC register is modified
;//--------------------------------------------------------------------
   .macro   VectorDotProductACC  dotproduct,vector1,vector2
   max            Vector1111,  vf00,        vf00[w]
   mul            vclsmlftemp, \vector1,    \vector2
   add.x          acc,         vclsmlftemp, vclsmlftemp[y]
   madd.x         \dotproduct, Vector1111,  vclsmlftemp
   .endm

;//--------------------------------------------------------------------
;// VectorCrossProduct - Calculate the cross product of "vector1" and
;// "vector2", and output to "vectoroutput"
;//
;// Note: ACC register is modified
;//--------------------------------------------------------------------
   .macro   VectorCrossProduct  vectoroutput,vector1,vector2
   opmula.xyz     ACC,           \vector1, \vector2
   opmsub.xyz     \vectoroutput, \vector2, \vector1
   sub.w          \vectoroutput, vf00,     vf00
   .endm

;//--------------------------------------------------------------------
;// VectorNormalize - Bring the length of "vector" to 1.f, and output
;// it to "vectoroutput"
;//
;// Note: Q register is modified
;//--------------------------------------------------------------------
   .macro   VectorNormalize   vecoutput,vector
   mul.xyz        vclsmlftemp, \vector,     \vector
   add.x          vclsmlftemp, vclsmlftemp, vclsmlftemp[y]
   add.x          vclsmlftemp, vclsmlftemp, vclsmlftemp[z]
   rsqrt          q,           vf00[w],     vclsmlftemp[x]
   sub.w          \vecoutput,  vf00,        vf00
   mul.xyz        \vecoutput,  \vector,     q
   .endm

;//--------------------------------------------------------------------
;// VectorNormalizeXYZ - Bring the length of "vector" to 1.f, and out-
;// put it to "vectoroutput".  The "w" field isn't transfered.
;//
;// Note: Q register is modified
;//--------------------------------------------------------------------
   .macro   VectorNormalizeXYZ   vecoutput,vector
   mul.xyz        vclsmlftemp, \vector,     \vector
   add.x          vclsmlftemp, vclsmlftemp, vclsmlftemp[y]
   add.x          vclsmlftemp, vclsmlftemp, vclsmlftemp[z]
   rsqrt          q,           vf00[w],     vclsmlftemp[x]
   mul.xyz        \vecoutput,  \vector,     q
   .endm

;//--------------------------------------------------------------------
;// VertexLightAmb - Apply ambient lighting "ambientrgba" to a vertex
;// of color "vertexrgba", and output the result in "outputrgba"
;//--------------------------------------------------------------------
   .macro   VertexLightAmb rgbaout,vertexrgba,ambientrgba
   mul            \rgbaout, \vertexrgba, \ambientrgba
   .endm

;//--------------------------------------------------------------------
;// VertexLightDir3 - Apply up to 3 directional lights contained in a
;// light matrix "lightmatrix" to a vertex of color "vertexrgba" and
;// having a normal "vertexnormal", and output the result in
;// "outputrgba"
;//
;// Note: ACC register is modified
;//--------------------------------------------------------------------
   .macro   VertexLightDir3   rgbaout,vertexrgba,vertexnormal,lightcolors,lightnormals
   mul            acc,      \lightnormals[0], \vertexnormal[x]
   madd           acc,      \lightnormals[1], \vertexnormal[y]
   madd           acc,      \lightnormals[2], \vertexnormal[z]
   madd           \rgbaout, \lightnormals[3], \vertexnormal[w] ;// Here "rgbaout" is the dot product for the 3 lights
   max            \rgbaout, \rgbaout,         vf00[x]          ;// Here "rgbaout" is the dot product for the 3 lights
   mul            acc,      \lightcolors[0],  \rgbaout[x]
   madd           acc,      \lightcolors[1],  \rgbaout[y]
   madd           \rgbaout, \lightcolors[2],  \rgbaout[z]      ;// Here "rgbaout" is the light applied on the vertex
   mul            \rgbaout, \vertexrgba,      \rgbaout         ;// Here "rgbaout" is the amount of light reflected by the vertex
   .endm

;//--------------------------------------------------------------------
;// VertexLightDir3Amb - Apply up to 3 directional lights, plus an
;// ambient light contained in a light matrix "lightmatrix" to a vertex
;// of color "vertexrgba" and having a normal "vertexnormal", and
;// output the result in "outputrgba"
;//
;// Note: ACC register is modified
;//--------------------------------------------------------------------
   .macro   VertexLightDir3Amb   rgbaout,vertexrgba,vertexnormal,lightcolors,lightnormals
   mul            acc,      \lightnormals[0], \vertexnormal[x]
   madd           acc,      \lightnormals[1], \vertexnormal[y]
   madd           acc,      \lightnormals[2], \vertexnormal[z]
   madd           \rgbaout, \lightnormals[3], \vertexnormal[w] ;// Here "rgbaout" is the dot product for the 3 lights
   max            \rgbaout, \rgbaout,         vf00[x]          ;// Here "rgbaout" is the dot product for the 3 lights
   mul            acc,      \lightcolors[0],  \rgbaout[x]
   madd           acc,      \lightcolors[1],  \rgbaout[y]
   madd           acc,      \lightcolors[2],  \rgbaout[z]
   madd           \rgbaout, \lightcolors[3],  \rgbaout[w]      ;// Here "rgbaout" is the light applied on the vertex
   mul.xyz        \rgbaout, \vertexrgba,      \rgbaout         ;// Here "rgbaout" is the amount of light reflected by the vertex
   .endm

;//--------------------------------------------------------------------
;// FogSetup - Set up fog "fogparams", by specifying "nearfog" and
;// "farfog".  "fogparams" will afterward be ready to be used by fog-
;// related macros, like "VertexFogLinear" for example.
;//
;// Note: I register is modified
;//--------------------------------------------------------------------
   .macro   FogSetup fogparams,nearfogz,farfogz
   sub            \fogparams, vf00,          vf00           ;// Set XYZW to 0
   loi            \farfogz                                  ;//
   add.w          \fogparams, \fogparams,    i              ;// fogparam[w] is farfogz
   loi            \nearfogz
   add.z          \fogparams, \fogparams,    \fogparams[w]
   sub.z          \fogparams, \fogparams,    i
   loi            255.0
   add.xy         \fogparams, \fogparams,    i              ;// fogparam[y] is 255.0
   sub.x          \fogparams, \fogparams,    vf00[w]        ;// fogparam[x] is 254.0
   div            q,          \fogparams[y], \fogparams[z]
   sub.z          \fogparams, \fogparams,    \fogparams
   add.z          \fogparams, \fogparams,    q              ;// fogparam[z] is 255.f / (farfogz - nearfogz)
   .endm

;//--------------------------------------------------------------------
;// VertexFogLinear - Apply fog "fogparams" to a vertex "xyzw", and
;// output the result in "xyzfoutput". "xyzw" [w] is assumed to be
;// the distance from the camera.  "fogparams" must contain farfogz in
;// [w], and (255.f / (farfogz - nearfogz)) in [z]. "xyzfoutputf" [w]
;// will contain a float value between 0.0 and 255.0, inclusively.
;//--------------------------------------------------------------------
   .macro   VertexFogLinear   xyzfoutput,xyzw,fogparams
   move.xyz       \xyzfoutput, \xyzw                        ;// XYZ part won't be modified
   sub.w          \xyzfoutput, \fogparams,  \xyzw[w]        ;// fog = (farfogz - z) * 255.0 /
   mul.w          \xyzfoutput, \xyzfoutput, \fogparams[z]   ;//       (farfogz - nearfogz)
   max.w          \xyzfoutput, \xyzfoutput, vf00[x]         ;// Clamp fog values outside the range 0.0-255.0
   mini.w         \xyzfoutput, \xyzfoutput, \fogparams[y]   ;//
   .endm

;//--------------------------------------------------------------------
;// VertexFogRemove - Remove any effect of fog to "xyzf".  "fogparams"
;// [x] must be set to 254.0.  "xyzf" will be modified directly.
;//--------------------------------------------------------------------
   .macro   VertexFogRemove   xyzf,fogparams
   add.w          \xyzf, vf00, \fogparams[x]                ;// xyzw[w] = 1.0 + 254.0 = 255.0 = no fog
   .endm

;//--------------------------------------------------------------------
;// PushInteger1 - Push "integer1" on "stackptr"
;//
;// Note: "stackptr" is updated
;//--------------------------------------------------------------------
   .macro   PushInteger1   stackptr,integer1
   isubiu         \stackptr, \stackptr, 1
   iswr.x         \integer1, (\stackptr):VCLSML_STACK
   .endm

;//--------------------------------------------------------------------
;// PushInteger2 - Push "integer1" and "integer2" on "stackptr"
;//
;// Note: "stackptr" is updated
;//--------------------------------------------------------------------
   .macro   PushInteger2   stackptr,integer1,integer2
   isubiu         \stackptr, \stackptr, 1
   iswr.x         \integer1, (\stackptr):VCLSML_STACK
   iswr.y         \integer2, (\stackptr):VCLSML_STACK
   .endm

;//--------------------------------------------------------------------
;// PushInteger3 - Push "integer1", "integer2" and "integer3" on
;// "stackptr"
;//
;// Note: "stackptr" is updated
;//--------------------------------------------------------------------
   .macro   PushInteger3   stackptr,integer1,integer2,integer3
   isubiu         \stackptr, \stackptr, 1
   iswr.x         \integer1, (\stackptr):VCLSML_STACK
   iswr.y         \integer2, (\stackptr):VCLSML_STACK
   iswr.z         \integer3, (\stackptr):VCLSML_STACK
   .endm

;//--------------------------------------------------------------------
;// PushInteger4 - Push "integer1", "integer2", "integer3" and
;// "integer4" on "stackptr"
;//
;// Note: "stackptr" is updated
;//--------------------------------------------------------------------
   .macro   PushInteger4   stackptr,integer1,integer2,integer3,integer4
   isubiu         \stackptr, \stackptr, 1
   iswr.x         \integer1, (\stackptr):VCLSML_STACK
   iswr.y         \integer2, (\stackptr):VCLSML_STACK
   iswr.z         \integer3, (\stackptr):VCLSML_STACK
   iswr.w         \integer4, (\stackptr):VCLSML_STACK
   .endm

;//--------------------------------------------------------------------
;// PopInteger1 - Pop "integer1" on "stackptr"
;//
;// Note: "stackptr" is updated
;//--------------------------------------------------------------------
   .macro   PopInteger1   stackptr,integer1
   ilwr.x         \integer1, (\stackptr):VCLSML_STACK
   iaddiu         \stackptr, \stackptr, 1
   .endm

;//--------------------------------------------------------------------
;// PopInteger2 - Pop "integer1" and "integer2" on "stackptr"
;//
;// Note: "stackptr" is updated
;//--------------------------------------------------------------------
   .macro   PopInteger2   stackptr,integer1,integer2
   ilwr.y         \integer2, (\stackptr):VCLSML_STACK
   ilwr.x         \integer1, (\stackptr):VCLSML_STACK
   iaddiu         \stackptr, \stackptr, 1
   .endm

;//--------------------------------------------------------------------
;// PopInteger3 - Pop "integer1", "integer2" and "integer3" on
;// "stackptr"
;//
;// Note: "stackptr" is updated
;//--------------------------------------------------------------------
   .macro   PopInteger3   stackptr,integer1,integer2,integer3
   ilwr.z         \integer3, (\stackptr):VCLSML_STACK
   ilwr.y         \integer2, (\stackptr):VCLSML_STACK
   ilwr.x         \integer1, (\stackptr):VCLSML_STACK
   iaddiu         \stackptr, \stackptr, 1
   .endm

;//--------------------------------------------------------------------
;// PopInteger4 - Pop "integer1", "integer2", "integer3" and
;// "integer4" on "stackptr"
;//
;// Note: "stackptr" is updated
;//--------------------------------------------------------------------
   .macro   PopInteger4   stackptr,integer1,integer2,integer3,integer4
   ilwr.w         \integer4, (\stackptr):VCLSML_STACK
   ilwr.z         \integer3, (\stackptr):VCLSML_STACK
   ilwr.y         \integer2, (\stackptr):VCLSML_STACK
   ilwr.x         \integer1, (\stackptr):VCLSML_STACK
   iaddiu         \stackptr, \stackptr, 1
   .endm

;//--------------------------------------------------------------------
;// PushMatrix - Push "matrix" onto the "stackptr"
;//
;// Note: "stackptr" is updated
;//--------------------------------------------------------------------
   .macro   PushMatrix  stackptr,matrix
   sq             \matrix[0], -1(\stackptr):VCLSML_STACK
   sq             \matrix[1], -2(\stackptr):VCLSML_STACK
   sq             \matrix[2], -3(\stackptr):VCLSML_STACK
   sq             \matrix[3], -4(\stackptr):VCLSML_STACK
   iaddi          \stackptr, \stackptr, -4
   .endm

;//--------------------------------------------------------------------
;// PopMatrix - Pop "matrix" out of the "stackptr"
;//
;// Note: "stackptr" is updated
;//--------------------------------------------------------------------
   .macro   PopMatrix   stackptr,matrix
   lq             \matrix[0], 0(\stackptr):VCLSML_STACK
   lq             \matrix[1], 1(\stackptr):VCLSML_STACK
   lq             \matrix[2], 2(\stackptr):VCLSML_STACK
   lq             \matrix[3], 3(\stackptr):VCLSML_STACK
   iaddi          \stackptr, \stackptr, 4
   .endm

;//--------------------------------------------------------------------
;// PushVector - Push "vector" onto the "stackptr"
;//
;// Note: "stackptr" is updated
;//--------------------------------------------------------------------
   .macro   PushVector  stackptr,vector
   sqd            \vector, (--\stackptr):VCLSML_STACK
   .endm

;//--------------------------------------------------------------------
;// PopVector - Pop "vector" out of the "stackptr"
;//
;// Note: "stackptr" is updated
;//--------------------------------------------------------------------
   .macro   PopVector   stackptr,vector
   lqi            \vector, (\stackptr++):VCLSML_STACK
   .endm

;//--------------------------------------------------------------------
;// PushVertex - Push "vector" onto the "stackptr"
;//
;// Note: "stackptr" is updated
;//--------------------------------------------------------------------
   .macro   PushVertex  stackptr,vertex
   sqd            \vertex, (--\stackptr):VCLSML_STACK
   .endm

;//--------------------------------------------------------------------
;// PopVertex - Pop "vertex" out of the "stackptr"
;//
;// Note: "stackptr" is updated
;//--------------------------------------------------------------------
   .macro   PopVertex   stackptr,vertex
   lqi            \vertex, (\stackptr++):VCLSML_STACK
   .endm

;//--------------------------------------------------------------------
;// AngleSinCos - Returns the sin and cos of up to 2 angles, which must
;// be contained in the X and Z elements of "angle".  The sin/cos pair
;// will be contained in the X/Y elements of "sincos" for the first
;// angle, and Z/W for the second one.
;// Thanks to Colin Hugues (SCEE) for that one
;//
;// Note: ACC and I registers are modified, and a bunch of temporary
;//       variables are created... Maybe bad for VCL register pressure
;//--------------------------------------------------------------------
   .macro   AngleSinCos    angle,sincos
   move.xz        \sincos, \angle                           ; To avoid modifying the original angles...

   mul.w          \sincos, vf00, \sincos[z]                 ; Copy angle from z to w
   add.y          \sincos, vf00, \sincos[x]                 ; Copy angle from x to y

   loi            1.570796                                  ; Phase difference for sin as cos ( PI/2 )
   sub.xz         \sincos, \sincos, I                       ;

   abs            \sincos, \sincos                          ; Mirror cos around zero

   max            Vector1111, vf00, vf00[w]                 ; Initialise all 1s

   loi            -0.159155                                 ; Scale so single cycle is range 0 to -1 ( *-1/2PI )
   mul            ACC,  \sincos, I                          ;

   loi            12582912.0                                ; Apply bias to remove fractional part
   msub           ACC,  Vector1111, I                       ;
   madd           ACC,  Vector1111, I                       ; Remove bias to leave original int part

   loi            -0.159155                                 ; Apply original number to leave fraction range only
   msub           ACC,  \sincos, I                          ;

   loi            0.5                                       ; Ajust range: -0.5 to +0.5
   msub           \sincos, Vector1111, I                    ;

   abs            \sincos, \sincos                          ; Clamp: 0 to +0.5

   loi            0.25                                      ; Ajust range: -0.25 to +0.25
   sub            \sincos, \sincos, I                       ;

   mul            anglepower2, \sincos, \sincos             ; a^2

   loi            -76.574959                                ;
   mul            k4angle, \sincos, I                       ; k4 a

   loi            -41.341675                                ;
   mul            k2angle, \sincos, I                       ; k2 a

   loi            81.602226                                 ;
   mul            k3angle, \sincos, I                       ; k3 a

   mul            anglepower4, anglepower2, anglepower2     ; a^4
   mul            k4angle, k4angle, anglepower2             ; k4 a^3
   mul            ACC,  k2angle, anglepower2                ; + k2 a^3

   loi            39.710659                                 ; k5 a
   mul            k2angle, \sincos, I                       ;

   mul            anglepower8, anglepower4, anglepower4     ; a^8
   madd           ACC,  k4angle, anglepower4                ; + k4 a^7
   madd           ACC,  k3angle, anglepower4                ; + k3 a^5
   loi            6.283185                                  ;
   madd           ACC,  \sincos, I                          ; + k1 a
   madd           \sincos, k2angle, anglepower8             ; + k5 a^9
   .endm   

;//--------------------------------------------------------------------
;// QuaternionToMatrix - Converts a quaternion rotation to a matrix
;// Thanks to Colin Hugues (SCEE) for that one
;//
;// Note: ACC and I registers are modified
;//--------------------------------------------------------------------
   .macro   QuaternionToMatrix   matresult,quaternion

   mula.xyz       ACC,  \quaternion, \quaternion            ; xx yy zz

   loi            1.414213562
   muli           vclsmlftemp, \quaternion, I               ; x sqrt2 y sqrt2 z sqrt2 w sqrt2

   mr32.w         \matresult[0], vf00                       ; Set rhs matrix line 0 to 0
   mr32.w         \matresult[1], vf00                       ;
   mr32.w         \matresult[2], vf00                       ; Set rhs matrix
   move           \matresult[3], vf00                       ; Set bottom line to 0 0 0 1

   madd.xyz       vcl_2qq, \quaternion, \quaternion         ; 2xx       2yy       2zz
   addw.xyz       Vector111, vf00, vf00                     ; 1         1         1         -

   opmula.xyz     ACC,  vclsmlftemp, vclsmlftemp            ; 2yz       2xz       2xy       -
   msubw.xyz      vclsmlftemp2, vclsmlftemp, vclsmlftemp    ; 2yz-2xw   2xz-2yz   2xy-2zw   -
   maddw.xyz      vclsmlftemp3, vclsmlftemp, vclsmlftemp    ; 2yz+2xw   2xz+2yz   2xy+2zw   -
   addaw.xyz      ACC,  vf00, vf00                          ; 1         1         1         -
   msubax.yz      ACC,  Vector111, vcl_2qq                  ; 1         1-2xx     1-2xx

   msuby.z        \matresult[2], Vector111, vcl_2qq         ; -         -         1-2xx-2yy -
   msubay.x       ACC, Vector111, vcl_2qq                   ; 1-2yy     1-2xx     1-2xx-2yy -
   msubz.y        \matresult[1], Vector111, vcl_2qq         ; -         1-2xx-2zz -         -
   mr32.y         \matresult[0], vclsmlftemp2
   msubz.x        \matresult[0], Vector111, vcl_2qq         ; 1-2yy-2zz -         -         -
   mr32.x         \matresult[2], vclsmlftemp2
   addy.z         \matresult[0], vf00, vclsmlftemp3
   mr32.w         vclsmlftemp, vclsmlftemp2
   mr32.z         \matresult[1], vclsmlftemp
   addx.y         \matresult[2], vf00, vclsmlftemp3
   mr32.y         vclsmlftemp3, vclsmlftemp3
   mr32.x         \matresult[1], vclsmlftemp3

   .endm

;//--------------------------------------------------------------------
;// QuaternionMultiply - Multiplies "quaternion1" and "quaternion2",
;// and puts the result in "quatresult".
;// Thanks to Colin Hugues (SCEE) for that one
;//
;// Note: ACC register is modified
;//--------------------------------------------------------------------
   .macro   QuaternionMultiply   quatresult,quaternion1,quaternion2
   mul            vclsmlftemp, \quaternion1, \quaternion2   ; xx yy zz ww

   opmula.xyz     ACC,         \quaternion1, \quaternion2   ; Start Outerproduct
   madd.xyz       ACC,         \quaternion1, \quaternion2[w]; Add w2.xyz1
   madd.xyz       ACC,         \quaternion2, \quaternion1[w]; Add w1.xyz2
   opmsub.xyz     \quatresult, \quaternion2, \quaternion1   ; Finish Outerproduct

   sub.w          ACC,         vclsmlftemp,  vclsmlftemp[z] ; ww - zz
   msub.w         ACC,         vf00,         vclsmlftemp[y] ; ww - zz - yy
   msub.w         \quatresult, vf00,         vclsmlftemp[x] ; ww - zz - yy - xx
   .endm

;//--------------------------------------------------------------------
;// TriangleWinding - Compute winding of triangle relative to "eyepos"
;// result is nonzero if winding is CW (actually depends on your
;// coordinate system)
;// Thanks to David Etherton (Angel Studios) for that one
;//
;// Note: ACC register is modified
;//--------------------------------------------------------------------
   .macro         TriangleWinding   result, vert1, vert2, vert3, eyepos
   sub.xyz        tw_vert12, \vert2, \vert1
   sub.xyz        tw_vert13, \vert3, \vert1

   opmula.xyz     ACC,       tw_vert12, tw_vert13
   opmsub.xyz     tw_normal, tw_vert13, tw_vert12

   sub.xyz        tw_dot, \eyepos, \vert1

   mul.xyz        tw_dot, tw_dot, tw_normal
   add.x          tw_dot, tw_dot, tw_dot[y]
   add.x          tw_dot, tw_dot, tw_dot[z]

   fsand          \result,   0x2
   .endm

;//--------------------------------------------------------------------
;// STATUSFLAGS_BGTZ - Branch if status shows "greater than zero".
;// Thanks to David Etherton (Angel Studios) for that one
;//--------------------------------------------------------------------
   .macro         STATUSFLAGS_BGTZ label
   fsand          vclsmlitemp, 0x3              ; NEG | ZERO
   ibeq           vclsmlitemp, VI00, \label     ; Jump if NEITHER NEG NOR ZERO
   .endm

;//--------------------------------------------------------------------
;// STATUSFLAGS_BGEZ - Branch if status shows "greater or equal to
;// zero".
;// Thanks to David Etherton (Angel Studios) for that one
;//--------------------------------------------------------------------
   .macro         STATUSFLAGS_BGEZ label
   fsand          vclsmlitemp, 0x2              ; NEG
   ibeq           vclsmlitemp, VI00, \label     ; Jump if NOT NEG
   .endm

;//--------------------------------------------------------------------
;// STATUSFLAGS_BLEZ - Branch if status shows "less or equal to zero".
;// Thanks to David Etherton (Angel Studios) for that one
;//--------------------------------------------------------------------
   .macro         STATUSFLAGS_BLEZ label
   fsand          vclsmlitemp, 0x3              ; NEG | ZERO
   ibne           vclsmlitemp, VI00, \label     ; Jump if NEG OR ZERO
   .endm

;//--------------------------------------------------------------------
;// STATUSFLAGS_BLTZ - Branch if status shows "less than zero".
;// Thanks to David Etherton (Angel Studios) for that one
;//--------------------------------------------------------------------
   .macro         STATUSFLAGS_BLTZ label
   fsand          vclsmlitemp, 0x2              ; NEG
   ibne           vclsmlitemp, VI00, \label     ; Jump if NEG
   .endm

;//--------------------------------------------------------------------
;// Name Here - Description here
;//
;// Note:
;//--------------------------------------------------------------------

   .macro LoadFloat output, immediate
   loi            \immediate                            ;//
   addi           output,     vf00,      i              ;//
   .endm

;//--------------------------------------------------------------------
;// VectorClamp - Clamp vector to a range
;//
;// Note:
;//--------------------------------------------------------------------

   .macro   VectorClamp output, input, min, max
   loi            \min                                  ;//
   addi           minvec,  vf00,         i              ;//
   loi            \max                                  ;//
   addi           maxvec,  vf00,         i              ;//
   maxx.xyzw      \output, \input,  minvec              ;//
   minix.xyzw     \output, \output, maxvec              ;//
   .endm

;//--------------------------------------------------------------------
;// Name Here - Description here
;//
;// Note:
;//--------------------------------------------------------------------
