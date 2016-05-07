SHADERPATH='./src/client/refresh/'
SOURCE=$SHADERPATH'pathtracer.glsl'
DEST=$SHADERPATH'generated/pathtracing_shader_source.h'
mkdir -p $SHADERPATH'generated'
echo '/* This is an automatically-generated file. Do not edit! */' > $DEST
sed -r 's/\\/\\\\/g; s/^/"/g; s/$/\\n"/g' $SOURCE >> $DEST