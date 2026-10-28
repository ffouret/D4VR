;
; Input signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; POSITION                 0   xyzw        0     NONE   float   xyz
;
;
; Output signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; SV_Position              0   xyzw        0      POS   float   xyzw
;
; shader hash: 38605dfe8f74ec5d357b77ab7e8e6dbe
;
; Pipeline Runtime Information:
;
;PSVRuntimeInfo:
; Vertex Shader
; OutputPositionPresent=1
; MinimumExpectedWaveLaneCount: 0
; MaximumExpectedWaveLaneCount: 4294967295
; UsesViewID: false
; SigInputElements: 1
; SigOutputElements: 1
; SigPatchConstOrPrimElements: 0
; SigInputVectors: 1
; SigOutputVectors[0]: 1
; SigOutputVectors[1]: 0
; SigOutputVectors[2]: 0
; SigOutputVectors[3]: 0
; EntryFunctionName: main
;
;
; Input signature:
;
; Name                 Index             InterpMode DynIdx
; -------------------- ----- ---------------------- ------
; POSITION                 0
;
; Output signature:
;
; Name                 Index             InterpMode DynIdx
; -------------------- ----- ---------------------- ------
; SV_Position              0          noperspective
;
; Buffer Definitions:
;
; cbuffer SyntheticCamera
; {
;
;   struct hostlayout.SyntheticCamera
;   {
;
;       float4 padding[7];                            ; Offset:    0
;       row_major float4x4 viewProjection;            ; Offset:  112
;       float4 unrelated;                             ; Offset:  176
;
;   } SyntheticCamera;                                ; Offset:    0 Size:   192
;
; }
;
; cbuffer SyntheticObject
; {
;
;   struct hostlayout.SyntheticObject
;   {
;
;       float4 objectPadding[2];                      ; Offset:    0
;       row_major float4x4 modelView;                 ; Offset:   32
;
;   } SyntheticObject;                                ; Offset:    0 Size:    96
;
; }
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; SyntheticCamera                   cbuffer      NA          NA     CB0     cb3,space2     1
; SyntheticObject                   cbuffer      NA          NA     CB1     cb1,space2     1
;
;
; ViewId state:
;
; Number of inputs: 4, outputs: 4
; Outputs dependent on ViewId: {  }
; Inputs contributing to computation of Outputs:
;   output 0 depends on inputs: { 0, 1, 2 }
;   output 1 depends on inputs: { 0, 1, 2 }
;   output 2 depends on inputs: { 0, 1, 2 }
;   output 3 depends on inputs: { 0, 1, 2 }
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.CBufRet.f32 = type { float, float, float, float }
%hostlayout.SyntheticCamera = type { [7 x <4 x float>], [4 x <4 x float>], <4 x float> }
%hostlayout.SyntheticObject = type { [2 x <4 x float>], [4 x <4 x float>] }

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 1, i32 1, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 3, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %3 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %4 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %5 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %6 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %7 = extractvalue %dx.types.CBufRet.f32 %6, 0
  %8 = extractvalue %dx.types.CBufRet.f32 %6, 1
  %9 = extractvalue %dx.types.CBufRet.f32 %6, 2
  %10 = extractvalue %dx.types.CBufRet.f32 %6, 3
  %11 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %12 = extractvalue %dx.types.CBufRet.f32 %11, 0
  %13 = extractvalue %dx.types.CBufRet.f32 %11, 1
  %14 = extractvalue %dx.types.CBufRet.f32 %11, 2
  %15 = extractvalue %dx.types.CBufRet.f32 %11, 3
  %16 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %17 = extractvalue %dx.types.CBufRet.f32 %16, 0
  %18 = extractvalue %dx.types.CBufRet.f32 %16, 1
  %19 = extractvalue %dx.types.CBufRet.f32 %16, 2
  %20 = extractvalue %dx.types.CBufRet.f32 %16, 3
  %21 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %22 = extractvalue %dx.types.CBufRet.f32 %21, 0
  %23 = extractvalue %dx.types.CBufRet.f32 %21, 1
  %24 = extractvalue %dx.types.CBufRet.f32 %21, 2
  %25 = extractvalue %dx.types.CBufRet.f32 %21, 3
  %26 = fmul fast float %7, %3
  %27 = call float @dx.op.tertiary.f32(i32 46, float %8, float %4, float %26)  ; FMad(a,b,c)
  %28 = call float @dx.op.tertiary.f32(i32 46, float %9, float %5, float %27)  ; FMad(a,b,c)
  %29 = fadd fast float %28, %10
  %30 = fmul fast float %12, %3
  %31 = call float @dx.op.tertiary.f32(i32 46, float %13, float %4, float %30)  ; FMad(a,b,c)
  %32 = call float @dx.op.tertiary.f32(i32 46, float %14, float %5, float %31)  ; FMad(a,b,c)
  %33 = fadd fast float %32, %15
  %34 = fmul fast float %17, %3
  %35 = call float @dx.op.tertiary.f32(i32 46, float %18, float %4, float %34)  ; FMad(a,b,c)
  %36 = call float @dx.op.tertiary.f32(i32 46, float %19, float %5, float %35)  ; FMad(a,b,c)
  %37 = fadd fast float %36, %20
  %38 = fmul fast float %22, %3
  %39 = call float @dx.op.tertiary.f32(i32 46, float %23, float %4, float %38)  ; FMad(a,b,c)
  %40 = call float @dx.op.tertiary.f32(i32 46, float %24, float %5, float %39)  ; FMad(a,b,c)
  %41 = fadd fast float %40, %25
  %42 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %2, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %43 = extractvalue %dx.types.CBufRet.f32 %42, 0
  %44 = extractvalue %dx.types.CBufRet.f32 %42, 1
  %45 = extractvalue %dx.types.CBufRet.f32 %42, 2
  %46 = extractvalue %dx.types.CBufRet.f32 %42, 3
  %47 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %2, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %48 = extractvalue %dx.types.CBufRet.f32 %47, 0
  %49 = extractvalue %dx.types.CBufRet.f32 %47, 1
  %50 = extractvalue %dx.types.CBufRet.f32 %47, 2
  %51 = extractvalue %dx.types.CBufRet.f32 %47, 3
  %52 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %2, i32 9)  ; CBufferLoadLegacy(handle,regIndex)
  %53 = extractvalue %dx.types.CBufRet.f32 %52, 0
  %54 = extractvalue %dx.types.CBufRet.f32 %52, 1
  %55 = extractvalue %dx.types.CBufRet.f32 %52, 2
  %56 = extractvalue %dx.types.CBufRet.f32 %52, 3
  %57 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %2, i32 10)  ; CBufferLoadLegacy(handle,regIndex)
  %58 = extractvalue %dx.types.CBufRet.f32 %57, 0
  %59 = extractvalue %dx.types.CBufRet.f32 %57, 1
  %60 = extractvalue %dx.types.CBufRet.f32 %57, 2
  %61 = extractvalue %dx.types.CBufRet.f32 %57, 3
  %62 = fmul fast float %43, %29
  %63 = call float @dx.op.tertiary.f32(i32 46, float %44, float %33, float %62)  ; FMad(a,b,c)
  %64 = call float @dx.op.tertiary.f32(i32 46, float %45, float %37, float %63)  ; FMad(a,b,c)
  %65 = call float @dx.op.tertiary.f32(i32 46, float %46, float %41, float %64)  ; FMad(a,b,c)
  %66 = fmul fast float %48, %29
  %67 = call float @dx.op.tertiary.f32(i32 46, float %49, float %33, float %66)  ; FMad(a,b,c)
  %68 = call float @dx.op.tertiary.f32(i32 46, float %50, float %37, float %67)  ; FMad(a,b,c)
  %69 = call float @dx.op.tertiary.f32(i32 46, float %51, float %41, float %68)  ; FMad(a,b,c)
  %70 = fmul fast float %53, %29
  %71 = call float @dx.op.tertiary.f32(i32 46, float %54, float %33, float %70)  ; FMad(a,b,c)
  %72 = call float @dx.op.tertiary.f32(i32 46, float %55, float %37, float %71)  ; FMad(a,b,c)
  %73 = call float @dx.op.tertiary.f32(i32 46, float %56, float %41, float %72)  ; FMad(a,b,c)
  %74 = fmul fast float %58, %29
  %75 = call float @dx.op.tertiary.f32(i32 46, float %59, float %33, float %74)  ; FMad(a,b,c)
  %76 = call float @dx.op.tertiary.f32(i32 46, float %60, float %37, float %75)  ; FMad(a,b,c)
  %77 = call float @dx.op.tertiary.f32(i32 46, float %61, float %41, float %76)  ; FMad(a,b,c)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %65)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %69)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %73)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %77)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #0

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #1

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32, %dx.types.Handle, i32) #2

; Function Attrs: nounwind readnone
declare float @dx.op.tertiary.f32(i32, float, float, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.Handle @dx.op.createHandle(i32, i8, i32, i32, i1) #2

attributes #0 = { nounwind readnone }
attributes #1 = { nounwind }
attributes #2 = { nounwind readonly }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!2}
!dx.shaderModel = !{!3}
!dx.resources = !{!4}
!dx.viewIdState = !{!8}
!dx.entryPoints = !{!9}

!0 = !{!"dxcoob 1.9.2609.5 (01b62ad47)"}
!1 = !{i32 1, i32 0}
!2 = !{i32 1, i32 9}
!3 = !{!"vs", i32 6, i32 0}
!4 = !{null, null, !5, null}
!5 = !{!6, !7}
!6 = !{i32 0, %hostlayout.SyntheticCamera* undef, !"", i32 2, i32 3, i32 1, i32 192, null}
!7 = !{i32 1, %hostlayout.SyntheticObject* undef, !"", i32 2, i32 1, i32 1, i32 96, null}
!8 = !{[6 x i32] [i32 4, i32 4, i32 15, i32 15, i32 15, i32 0]}
!9 = !{void ()* @main, !"main", !10, !4, null}
!10 = !{!11, !15, null}
!11 = !{!12}
!12 = !{i32 0, !"POSITION", i8 9, i8 0, !13, i8 0, i32 1, i8 4, i32 0, i8 0, !14}
!13 = !{i32 0}
!14 = !{i32 3, i32 7}
!15 = !{!16}
!16 = !{i32 0, !"SV_Position", i8 9, i8 3, !13, i8 4, i32 1, i8 4, i32 0, i8 0, !17}
!17 = !{i32 3, i32 15}
