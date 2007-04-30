// This file is part of RLVM, a RealLive virtual machine clone.
//
// -----------------------------------------------------------------------
//
// Copyright (C) 2006 Elliot Glaysher
//  
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//  
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//  
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//  
// -----------------------------------------------------------------------

#include "glew.h"

#include <boost/bind.hpp>
#include <SDL/SDL.h>
#include <SDL/SDL_opengl.h>

#include <iostream>
#include <cstdio>
#include <sstream>
#include <fstream>
#include <string>
#include <algorithm>
#include <cmath>

#include "Systems/Base/SystemError.hpp"
#include "Systems/Base/GraphicsObject.hpp"
#include "Systems/SDL/SDLGraphicsSystem.hpp"
#include "Systems/SDL/SDLSurface.hpp"
#include "Systems/SDL/Texture.hpp"
#include "Systems/SDL/SDLUtils.hpp"

#include "Systems/Base/SystemError.hpp"

#include "alphablit.h"

using namespace std;
using namespace boost;

GLuint Texture::m_shaderObjectID = 0;
GLuint Texture::m_programObjectID = 0;

unsigned int Texture::s_screenWidth = 0;
unsigned int Texture::s_screenHeight = 0;

// -----------------------------------------------------------------------

void Texture::SetScreenSize(unsigned int width, unsigned int height)
{
  s_screenWidth = width;
  s_screenHeight = height;
}

// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
// Texture
// -----------------------------------------------------------------------
Texture::Texture(SDL_Surface* surface, int x, int y, int w, int h, 
                 GLenum bytesPerPixel, GLint byteOrder, GLint byteType)
  : m_xOffset(x), m_yOffset(y), m_logicalWidth(w), m_logicalHeight(h),
    m_totalWidth(surface->w), m_totalHeight(surface->h),     
    m_backTextureID(0), m_isUpsideDown(false)
{
  glGenTextures(1, &m_textureID);
  glBindTexture(GL_TEXTURE_2D, m_textureID);
  ShowGLErrors();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // Build a surface of this part of the 
  SDL_Surface* tmpSurface =
    SDL_CreateRGBSurface(surface->flags, w, h,
                         surface->format->BitsPerPixel,
                         surface->format->Rmask, surface->format->Gmask,
                         surface->format->Bmask, surface->format->Amask);
  if(tmpSurface == NULL)
    reportSDLError("SDL_CreateRGBSurface", "Texture::Texture()");

  SDL_Rect rect;
  rect.x = x; rect.y = y; rect.w = w; rect.h = h;
  if(pygame_AlphaBlit(surface, &rect, tmpSurface, NULL))
    reportSDLError("pygame_AlphaBlit", "Texture::Texture()");

  SDL_LockSurface(tmpSurface);

  m_textureWidth = SafeSize(m_logicalWidth);
  m_textureHeight = SafeSize(m_logicalHeight);
  glTexImage2D(GL_TEXTURE_2D, 0, bytesPerPixel,
               m_textureWidth, m_textureHeight,
               0, 
               byteOrder, byteType, NULL);
  ShowGLErrors();

  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tmpSurface->w, tmpSurface->h,
                  byteOrder, byteType, tmpSurface->pixels);            
  ShowGLErrors();

  SDL_UnlockSurface(tmpSurface);
  SDL_FreeSurface(tmpSurface);
}

// -----------------------------------------------------------------------

Texture::Texture(render_to_texture, int width, int height)
  : m_xOffset(0), m_yOffset(0),
    m_logicalWidth(width), m_logicalHeight(height), 
    m_totalWidth(width), m_totalHeight(height),     
    m_textureWidth(0), m_textureHeight(0), m_textureID(0),
    m_backTextureID(0), m_isUpsideDown(true)
{
  glGenTextures(1, &m_textureID);
  glBindTexture(GL_TEXTURE_2D, m_textureID);
  ShowGLErrors();
//  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
//  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  m_textureWidth = SafeSize(m_logicalWidth);
  m_textureHeight = SafeSize(m_logicalHeight);

  // This may fail.
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
               m_textureWidth, m_textureHeight,
               0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
  ShowGLErrors();

  glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, m_logicalWidth, 
                      m_logicalHeight);
  ShowGLErrors();
}

// -----------------------------------------------------------------------

Texture::~Texture()
{
  glDeleteTextures(1, &m_textureID);
//  cerr << "Deleteing texture with texid " << m_textureID << endl;

  if(m_backTextureID)
    glDeleteTextures(1, &m_backTextureID);

  ShowGLErrors();
}

// -----------------------------------------------------------------------

std::string readTextFile(const std::string& file)
{
  ifstream ifs(file.c_str());
  if(!ifs)
  {
    ostringstream oss;
    oss << "Can't open text file: " << file;
    throw SystemError(oss.str());
  }

  string out, line;
  while(getline(ifs, line))
  {
    out += line;
    out += '\n';
  }

  return out;
}

// -----------------------------------------------------------------------

void printARBLog(GLhandleARB obj)
{
  char str[256];
  GLsizei size = 0;
  glGetInfoLogARB(obj, 256, &size, str);
  if(size != 0)
  {
    cerr << "Log: " << str << endl;
  }
}

// -----------------------------------------------------------------------

string Texture::getSubtractiveShaderString()
{
  string x =
    "uniform sampler2D currentValues, mask;"
    ""
    "void main()"
    "{"
    "vec4 bgColor = texture2D(currentValues, gl_TexCoord[0].st);"
    "vec4 maskVector = texture2D(mask, gl_TexCoord[1].st);"
    "float maskColor = clamp(maskVector.a * gl_Color.a, 0.0, 1.0);"
    "gl_FragColor = clamp(bgColor - maskColor + gl_Color * maskColor, 0.0, 1.0);"
    "}";

  return x;
}

// -----------------------------------------------------------------------

void Texture::buildShader()
{
  m_shaderObjectID = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
  ShowGLErrors();

//  string str = readTextFile("/Users/elliot/Projects/rlvm/src/Systems/SDL/subtractive.frag");
  string str = getSubtractiveShaderString();
  const char* file = str.c_str();

  glShaderSourceARB(m_shaderObjectID, 1, &file, NULL);
  ShowGLErrors();

  glCompileShaderARB(m_shaderObjectID);
  printARBLog(m_shaderObjectID);
  ShowGLErrors();

  // Check the log here

  m_programObjectID = glCreateProgramObjectARB();
  glAttachObjectARB(m_programObjectID, m_shaderObjectID);
  ShowGLErrors();

  glLinkProgramARB(m_programObjectID);
  printARBLog(m_programObjectID);
  ShowGLErrors();
}

// -----------------------------------------------------------------------

// This is really broken and brain dead.
void Texture::renderToScreen(int x1, int y1, int x2, int y2,
                             int dx1, int dy1, int dx2, int dy2,
                             int opacity)
{
  if(!filterCoords(x1, y1, x2, y2, dx1, dy1, dx2, dy2))
    return;

  // For the time being, we are dumb and assume that it's one texture
  
  float thisx1 = float(x1) / m_textureWidth;
  float thisy1 = float(y1) / m_textureHeight;
  float thisx2 = float(x2) / m_textureWidth;
  float thisy2 = float(y2) / m_textureHeight;

   if(m_isUpsideDown)
   {
     thisy1 = float(m_logicalHeight - y1) / m_textureHeight;
     thisy2 = float(m_logicalHeight - y2) / m_textureHeight;
   }

  glBindTexture(GL_TEXTURE_2D, m_textureID);

  // Blend when we have less opacity
//  if(opacity < 255)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glBegin(GL_QUADS);
  {
    glColor4ub(255, 255, 255, opacity);
    glTexCoord2f(thisx1, thisy1);
    glVertex2i(dx1, dy1);
    glTexCoord2f(thisx2, thisy1);
    glVertex2i(dx2, dy1);
    glTexCoord2f(thisx2, thisy2);
    glVertex2i(dx2, dy2);        
    glTexCoord2f(thisx1, thisy2);
    glVertex2i(dx1, dy2);
  }
  glEnd();
  glBlendFunc(GL_ONE, GL_ZERO);
}

// -----------------------------------------------------------------------

/** 
 * @todo A function of this hairiness needs super more amounts of
 *       documentation.
 * @todo When I merge back to trunk, make sure to change the throw
 *       cstrs over to the new exception class.
 */
void Texture::renderToScreenAsColorMask(
  int x1, int y1, int x2, int y2,
  int dx1, int dy1, int dx2, int dy2,
  int r, int g, int b, int alpha, int filter)
{
  if(filter == 0)
  {
    if(GLEW_ARB_fragment_shader && GLEW_ARB_multitexture)
    {
      renderToScreenAsColorMask_subtractive_glsl(
        x1, y1, x2, y2,
        dx1, dy1, dx2, dy2, r, g, b, alpha);
    }
    else
    {
      renderToScreenAsColorMask_subtractive_fallback(
        x1, y1, x2, y2,
        dx1, dy1, dx2, dy2, r, g, b, alpha);
    }
  }
  else
  {
    renderToScreenAsColorMask_additive(
        x1, y1, x2, y2,
        dx1, dy1, dx2, dy2, r, g, b, alpha);      
  }
}

// -----------------------------------------------------------------------

void Texture::renderToScreenAsColorMask_subtractive_glsl(
  int x1, int y1, int x2, int y2,
  int dx1, int dy1, int dx2, int dy2,
  int r, int g, int b, int alpha)
{
  if(!filterCoords(x1, y1, x2, y2, dx1, dy1, dx2, dy2))
    return;

  if(m_shaderObjectID == 0)
    buildShader();
  
  float thisx1 = float(x1) / m_textureWidth;
  float thisy1 = float(y1) / m_textureHeight;
  float thisx2 = float(x2) / m_textureWidth;
  float thisy2 = float(y2) / m_textureHeight;

  if(m_isUpsideDown)
  {
    thisy1 = float(m_logicalHeight - y1) / m_textureHeight;
    thisy2 = float(m_logicalHeight - y2) / m_textureHeight;
  }

  // If we haven't already, allocate video memory for the back
  // texture. 
  //
  // NOTE: Does this code deal with changing the dimensions of the
  // text box? Does it matter?
  if(m_backTextureID == 0)
  {
    glGenTextures(1, &m_backTextureID);
    glBindTexture(GL_TEXTURE_2D, m_backTextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Generate this texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 m_textureWidth, m_textureHeight,
                 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    ShowGLErrors();
  }

  // Copy the current value of the region where we're going to render
  // to a texture for input to the shader
  glBindTexture(GL_TEXTURE_2D, m_backTextureID);
  int ystart = s_screenHeight - dy1 - (dy2 - dy1);
  glCopyTexSubImage2D(GL_TEXTURE_2D, 
                      0,
                      0, 0, 
                      dx1, ystart, m_textureWidth, m_textureHeight);
  ShowGLErrors();

  glUseProgramObjectARB(m_programObjectID);

  // Put the backTexture in texture slot zero and set this to be the
  // texture "currentValues" in the above shader program.
  glActiveTextureARB(GL_TEXTURE0_ARB);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, m_backTextureID);
  GLint currentValuesLoc = glGetUniformLocationARB(m_programObjectID,
                                                   "currentValues");
  if(currentValuesLoc == -1)
    throw SystemError("Bad uniform value");
  glUniform1iARB(currentValuesLoc, 0);

  // Put the mask in texture slot one and set this to be the
  // texture "mask" in the above shader program.
  glActiveTextureARB(GL_TEXTURE1_ARB);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, m_textureID);
  GLint maskLoc = glGetUniformLocationARB(m_programObjectID, "mask");
  if(maskLoc == -1)
    throw SystemError("Bad uniform value");
  glUniform1iARB(maskLoc, 1);

  glDisable(GL_BLEND);

  glBegin(GL_QUADS);
  {
    glColor4ub(r, g, b, alpha);
    glMultiTexCoord2fARB(GL_TEXTURE0_ARB, thisx1, thisy2);
    glMultiTexCoord2fARB(GL_TEXTURE1_ARB, thisx1, thisy1);
    glVertex2i(dx1, dy1);
    glMultiTexCoord2fARB(GL_TEXTURE0_ARB, thisx2, thisy2);
    glMultiTexCoord2fARB(GL_TEXTURE1_ARB, thisx2, thisy1);
    glVertex2i(dx2, dy1);
    glMultiTexCoord2fARB(GL_TEXTURE0_ARB, thisx2, thisy1);
    glMultiTexCoord2fARB(GL_TEXTURE1_ARB, thisx2, thisy2);
    glVertex2i(dx2, dy2);        
    glMultiTexCoord2fARB(GL_TEXTURE0_ARB, thisx1, thisy1);
    glMultiTexCoord2fARB(GL_TEXTURE1_ARB, thisx1, thisy2);
    glVertex2i(dx1, dy2);
  }
  glEnd();

  glActiveTextureARB(GL_TEXTURE1_ARB);
  glDisable(GL_TEXTURE_2D);
  glActiveTextureARB(GL_TEXTURE0_ARB);

  glUseProgramObjectARB(0);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ZERO);
}

// -----------------------------------------------------------------------

/**
 * This fallback does not accurately render the scene according to
 * standard RealLive. This only negatively shades according to the
 * alpha value, ignoring the rest of the #WINDOW_ATTR color.
 * 
 * This will probably only occur with mesa software and people with
 * graphics cards > 5 years old.
 */
void Texture::renderToScreenAsColorMask_subtractive_fallback(
  int x1, int y1, int x2, int y2,
  int dx1, int dy1, int dx2, int dy2,
  int r, int g, int b, int alpha)
{
  if(!filterCoords(x1, y1, x2, y2, dx1, dy1, dx2, dy2))
    return;

  float thisx1 = float(x1) / m_textureWidth;
  float thisy1 = float(y1) / m_textureHeight;
  float thisx2 = float(x2) / m_textureWidth;
  float thisy2 = float(y2) / m_textureHeight;

  if(m_isUpsideDown)
  {
    thisy1 = float(m_logicalHeight - y1) / m_textureHeight;
    thisy2 = float(m_logicalHeight - y2) / m_textureHeight;
  }

  // First draw the mask
  glBindTexture(GL_TEXTURE_2D, m_textureID);

  /// SERIOUS WTF: glBlendFuncSeparate causes a segmentation fault
  /// under the current i810 driver for linux.
//  glBlendFuncSeparate(GL_SRC_ALPHA_SATURATE, GL_ONE_MINUS_SRC_ALPHA,
//                      GL_SRC_COLOR, GL_ONE_MINUS_SRC_ALPHA);
  glBlendFunc(GL_SRC_ALPHA_SATURATE, GL_ONE_MINUS_SRC_ALPHA);

  glBegin(GL_QUADS);
  {
    glColor4ub(r, g, b, alpha);
    glTexCoord2f(thisx1, thisy1);
    glVertex2i(dx1, dy1);
    glTexCoord2f(thisx2, thisy1);
    glVertex2i(dx2, dy1);
    glTexCoord2f(thisx2, thisy2);
    glVertex2i(dx2, dy2);        
    glTexCoord2f(thisx1, thisy2);
    glVertex2i(dx1, dy2);
  }
  glEnd();

  glBlendFunc(GL_ONE, GL_ZERO);
}

// -----------------------------------------------------------------------

void Texture::renderToScreenAsColorMask_additive(
  int x1, int y1, int x2, int y2,
  int dx1, int dy1, int dx2, int dy2,
  int r, int g, int b, int alpha)
{
  if(!filterCoords(x1, y1, x2, y2, dx1, dy1, dx2, dy2))
    return;

  float thisx1 = float(x1) / m_textureWidth;
  float thisy1 = float(y1) / m_textureHeight;
  float thisx2 = float(x2) / m_textureWidth;
  float thisy2 = float(y2) / m_textureHeight;

  if(m_isUpsideDown)
  {
    thisy1 = float(m_logicalHeight - y1) / m_textureHeight;
    thisy2 = float(m_logicalHeight - y2) / m_textureHeight;
  }

  // First draw the mask
  glBindTexture(GL_TEXTURE_2D, m_textureID);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glBegin(GL_QUADS);
  {
    glColor4ub(r, g, b, alpha);
    glTexCoord2f(thisx1, thisy1);
    glVertex2i(dx1, dy1);
    glTexCoord2f(thisx2, thisy1);
    glVertex2i(dx2, dy1);
    glTexCoord2f(thisx2, thisy2);
    glVertex2i(dx2, dy2);        
    glTexCoord2f(thisx1, thisy2);
    glVertex2i(dx1, dy2);
  }
  glEnd();

  glBlendFunc(GL_ONE, GL_ZERO);
}

// -----------------------------------------------------------------------

void Texture::renderToScreen(int x1, int y1, int x2, int y2,
                             int dx1, int dy1, int dx2, int dy2,
                             const int opacity[4])
{
  // For the time being, we are dumb and assume that it's one texture
  if(!filterCoords(x1, y1, x2, y2, dx1, dy1, dx2, dy2))
    return;
  
  float thisx1 = float(x1) / m_textureWidth;
  float thisy1 = float(y1) / m_textureHeight;
  float thisx2 = float(x2) / m_textureWidth;
  float thisy2 = float(y2) / m_textureHeight;

  glBindTexture(GL_TEXTURE_2D, m_textureID);

  // Blend when we have less opacity
  if(find_if(opacity, opacity + 4, bind(std::less<int>(), _1, 255)) 
     != opacity + 4)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glBegin(GL_QUADS);
  {
    glColor4ub(255, 255, 255, opacity[0]);
    glTexCoord2f(thisx1, thisy1);
    glVertex2i(dx1, dy1);
    glColor4ub(255, 255, 255, opacity[1]);
    glTexCoord2f(thisx2, thisy1);
    glVertex2i(dx2, dy1);
    glColor4ub(255, 255, 255, opacity[2]);
    glTexCoord2f(thisx2, thisy2);
    glVertex2i(dx2, dy2);        
    glColor4ub(255, 255, 255, opacity[3]);
    glTexCoord2f(thisx1, thisy2);
    glVertex2i(dx1, dy2);
  }
  glEnd();
  glBlendFunc(GL_ONE, GL_ZERO);
}

// -----------------------------------------------------------------------

void Texture::renderToScreenAsObject(const GraphicsObject& go, SDLSurface& surface)
{
  // Figure out the source to clip out of the image
  int pattNo = go.pattNo();
  int xSrc1 = surface.getPattern(pattNo).x1;
  int ySrc1 = surface.getPattern(pattNo).y1;
  int xSrc2 = surface.getPattern(pattNo).x2;
  int ySrc2 = surface.getPattern(pattNo).y2;

  // Figure out position to display on
  int xPos1 = go.x() + go.xAdjustmentSum();
  int yPos1 = go.y() + go.yAdjustmentSum();
  int xPos2 = int(xPos1 + (xSrc2 - xSrc1) * (go.width() / 100.0f));
  int yPos2 = int(yPos1 + (ySrc2 - ySrc1) * (go.height() / 100.0f));

  // If clipping is active for this object, take that into account too.
  if (go.hasClip()) {
    // Do nothing if object falls wholly outside clip area
    if (xPos2 < go.clipX1() || xPos1 > go.clipX2() ||
        yPos2 < go.clipY1() || yPos1 > go.clipY2()) {
      return;
    }
    // Otherwise, adjust coordinates to present only the visible area.
    if (xPos1 < go.clipX1()) {
      xSrc1 += go.clipX1() - xPos1;
      xPos1 = go.clipX1();
    }
    if (yPos1 < go.clipY1()) {
      ySrc1 += go.clipY1() - yPos1;
      yPos1 = go.clipY1();
    }
    if (xPos2 >= go.clipX2()) {
      xSrc2 -= xPos2 - go.clipX2();
      xPos2 = go.clipX2() + 1; // Yeah, more inclusive ranges. Hurray!
    }
    if (yPos2 >= go.clipY2()) {
      ySrc2 -= yPos2 - go.clipY2();
      yPos2 = go.clipY2() + 1;
    }
  }

  if(!filterCoords(xSrc1, ySrc1, xSrc2, ySrc2, xPos1, yPos1, xPos2, yPos2))
    return;
  
  // Convert the pixel coordinates into [0,1) texture coordinates
  float thisx1 = float(xSrc1) / m_textureWidth;
  float thisy1 = float(ySrc1) / m_textureHeight;
  float thisx2 = float(xSrc2) / m_textureWidth;
  float thisy2 = float(ySrc2) / m_textureHeight;

  glBindTexture(GL_TEXTURE_2D, m_textureID);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glPushMatrix();
  {
    // Move the "origin" to the correct position.
    glTranslatef(go.xOrigin(), go.yOrigin(), 0);

    // Rotate here?
    glRotatef(float(go.rotation()) / 10, 0, 0, 1);

    glBegin(GL_QUADS);
    {
      glColor4ub(go.tintR(), go.tintG(), go.tintB(), go.alpha());
      glTexCoord2f(thisx1, thisy1);
      glVertex2i(xPos1, yPos1);
      glTexCoord2f(thisx2, thisy1);
      glVertex2i(xPos2, yPos1);
      glTexCoord2f(thisx2, thisy2);
      glVertex2i(xPos2, yPos2);        
      glTexCoord2f(thisx1, thisy2);
      glVertex2i(xPos1, yPos2);
    }
    glEnd();

    glBlendFunc(GL_ONE, GL_ZERO);
  }
  glPopMatrix();

  ShowGLErrors();
}

// -----------------------------------------------------------------------

void Texture::rawRenderQuad(const int srcCoords[8], 
                            const int destCoords[8],
                            const int opacity[4])
{
  /// @bug FIXME!
//  if(!filterCoords(x1, y1, x2, y2, dx1, dy1, dx2, dy2))
//    return;

  // For the time being, we are dumb and assume that it's one texture
  float textureCoords[8];
  for(int i = 0; i < 8; i += 2)
  {
    textureCoords[i] = float(srcCoords[i]) / m_textureWidth;
    textureCoords[i + 1] = float(srcCoords[i + 1]) / m_textureHeight;
  }

  glBindTexture(GL_TEXTURE_2D, m_textureID);

  // Blend when we have less opacity
  if(find_if(opacity, opacity + 4, bind(std::less<int>(), _1, 255)) 
     != opacity + 4)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glBegin(GL_QUADS);
  {
    for(int i = 0; i < 4; i++)
    {
      glColor4ub(255, 255, 255, opacity[i]);

      int firstIndex = i * 2;
      int secondIndex = firstIndex + 1;
      glTexCoord2f(srcCoords[firstIndex], srcCoords[secondIndex]);
      glVertex2i(destCoords[firstIndex], destCoords[secondIndex]);
    }
  }
  glEnd();
  glBlendFunc(GL_ONE, GL_ZERO);
}

// -----------------------------------------------------------------------

bool Texture::filterCoords(int& x1, int& y1, int& x2, int& y2, int& dx1, 
                           int& dy1, int& dx2, int& dy2)
{
  using std::max;
  using std::min;

  // Input: raw image coordinates
  // Output: false if this doesn't intersect with the texture piece we hold.
  //         true otherwise, and set the local coordinates
  int w1 = x2 - x1;
  int w2 = m_logicalWidth;

  int h1 = y2 - y1;
  int h2 = m_logicalHeight;

  // First thing we do is an intersection test to see if this input
  // range intersects the virtual range this Texture object holds.
  //
  /// @bug s/>/>=/?
  if (x1 + w1 > m_xOffset && x1 < m_xOffset + m_logicalWidth &&
      y1 + h1 > m_yOffset && y1 < m_yOffset + m_logicalHeight)
  {
    // Do an intersection test in terms of the virtual coordinates
    int virX = max(x1, m_xOffset);
    int virY = max(y1, m_yOffset);
    int w = min(x1+w1, m_xOffset + m_logicalWidth) - max(x1, m_xOffset);
    int h = min(y1+h1, m_yOffset + m_logicalHeight) - max(y1, m_yOffset);

    // Adjust the destination coordinates
    int dxWidth = dx2 - dx1;
    int dyHeight = dy2 - dy1;
    float dx1Off = (virX - x1) / float(w1);
    dx1 = floor(dx1 + (dxWidth * dx1Off));
    float dx2Off = (w1 - w - (virX - x1)) / float(w1);
    dx2 = floor(dx2 - (dxWidth * dx2Off));
    float dy1Off = (virY - y1) / float(h1);
    dy1 = floor(dy1 + (dyHeight * dy1Off));
    float dy2Off = (h1 - h - (virY - y1)) / float(h1);
    dy2 = floor(dy2 - (dyHeight * dy2Off));

    // Output the source intersection in real (instead of
    // virtual) coordinates
    x1 = virX - m_xOffset;
    x2 = x1 + w;
    y1 = virY - m_yOffset;
    y2 = y1 + h;

    return true;
  }

  return false;
}
