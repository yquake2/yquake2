
#ifndef SRC_CLIENT_REFRESH_GL3_HEADER_QGL_H_
#define SRC_CLIENT_REFRESH_GL3_HEADER_QGL_H_

// this is just a hack to get proper auto-completion in IDEs:
// using system headers for their parsers/indexers but glad for real build
// (in glad glFoo is just a #define to glad_glFoo or sth, which screws up autocompletion)
// (you may have to configure your IDE to #define IN_IDE_PARSER, but not for building!)
#ifdef IN_IDE_PARSER
  #define GL_GLEXT_PROTOTYPES 1
  #include <GL/glext.h>
  #include <GL/gl.h>
#else
  #include <glad/glad.h>
#endif

#endif /* SRC_CLIENT_REFRESH_GL3_HEADER_QGL_H_ */
