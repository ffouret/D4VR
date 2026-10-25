#version 460
#extension GL_EXT_debug_printf : require
// A valid unmodeled instruction set must block the extended-effect audit.
layout(location=0) in float input_value;
layout(location=0) out float color;
void main() { debugPrintfEXT("audit value: %f",input_value); color=input_value; }
