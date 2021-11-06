#include <Shared/Shrink.h>
#include "GetText.h"
#include <Shared/Atlas.h>

int cacheSize = 0;
struct Translation {
  wchar_t *src;
  wchar_t *dst;
};
Translation *cache = 0;

int ColorClamp(int c) {
  if (c < 16) c = 16;
  if (c > 235) c = 235;
  return c;
}

int ColorDiff(unsigned int color1, unsigned int color2) {
  int c1 = color1;
  int c2 = color2;

  /*

  int r1 = ColorClamp((c1>>16) & 0xFF);
  int g1 = ColorClamp((c1>> 8) & 0xFF);
  int b1 = ColorClamp((c1>> 0) & 0xFF);

  int r2 = ColorClamp((c2>>16) & 0xFF);
  int g2 = ColorClamp((c2>> 8) & 0xFF);
  int b2 = ColorClamp((c2>> 0) & 0xFF);

  int y1 = (r1 * 299 + 587 * g1 + 114 * b1+500)/1000;
  int u1 = (713 * (r1 - y1)+500) / 1000 + 128;
  int v1 = (564 * (b1 - y1)+500) / 1000 + 128;

  int y2 = (r2 * 299 + 587 * g2 + 114 * b2+500)/1000;
  int u2 = (713 * (r2 - y2)+500) / 1000 + 128;
  int v2 = (564 * (b2 - y2)+500) / 1000 + 128;

  int dy = y1-y2;
  int du = u1-u2;
  int dv = v1-v2;
  return dy*dy + du*du + dv*dv;
  //*/

  int r1 = (c1>>16) & 0xFF;
  int g1 = (c1>> 8) & 0xFF;
  int b1 = (c1>> 0) & 0xFF;

  int r2 = (c2>>16) & 0xFF;
  int g2 = (c2>> 8) & 0xFF;
  int b2 = (c2>> 0) & 0xFF;

  int dr = ((c1>>16) & 0xFF)-((c2>>16) & 0xFF);
  int dg = ((c1>> 8) & 0xFF)-((c2>> 8) & 0xFF);
  int db = ((c1>> 0) & 0xFF)-((c2>> 0) & 0xFF);
  return dr*dr + dg*dg + db*db;
  //*/
}

int ColorDiff(unsigned int *bmp, int w, int x1, int y1, int x2, int y2) {
  return ColorDiff(bmp[w*y1+x1], bmp[w*y2+x2]);
}

int MyRGB(int r, int g, int b) {
  if (r > 255) r = 255;
  if (r < 0) r = 0;
  if (g > 255) g = 255;
  if (g < 0) g = 0;
  if (b > 255) b = 255;
  if (b < 0) b = 0;
  return (r << 16) + (g << 8) + b;
}
/*

int FindVStroke(unsigned int *bmp, int width, int dx, int dy, int x1, int y1, int x2, int y2) {
  int i, j;
  int res = 0;
  int dred = (*bmp>>16) & 0xFF;
  int dgreen = (*bmp>>8) & 0xFF;
  int dblue = (*bmp>>0) & 0xFF;
  for (j = y1*dy/9; j<=y2*dy/9; j++) {
    for (i = x1*dx/9; i<=x2*dx/9; i++) {
      int d = ColorDiff(bmp, width, i, j, 0, 0);
      if (d < 500) continue;
      int dd = ColorDiff(bmp, width, i+1, j, i, j);
      int c = bmp[width*(j-1) + i];
      int red = (c>>16) & 0xFF;
      int green = (c>>8) & 0xFF;
      int blue = (c>>0) & 0xFF;
      if (dd < 500) {
        // int _d1 = 
      }
      //int d2 = ColorDiff(bmp, width, i+1, j, 0, 0);
      //if (d2 < 500) {
        int c1 = bmp[width*(j-1) + i];
        int c2 = bmp[width*(j+1) + i];
        int r2 = (c2 >> 16)&0xFF;
        int g2 = (c2 >> 8)&0xFF;
        int b2 = (c2 >> 0)&0xFF;
        int dr = (((c1 >> 16)&0xFF) - ((c2 >> 16)&0xFF))/2;
        int dg = (((c1 >> 8)&0xFF) - ((c2 >> 8)&0xFF))/2;
        int db =  (((c1 >> 0)&0xFF) - ((c2 >> 0)&0xFF))/2;
        int color = MyRGB(red+dr, green+dg, blue+dg);
        int _d = ColorDiff(color, *bmp);
        int color2 = MyRGB(red-dr, green-dg, blue-dg);
        int _d2 = ColorDiff(color, *bmp);
        _d=_d;
        //int u = 
      //}
      int d3 = ColorDiff(bmp, width, i+2, j, 0, 0);
/*
        int _d3 = ColorDiff(bmp, width, 0, 0, i, j+1);
        int _d4 = ColorDiff(bmp, width, 0, 0, i, j-1);
        //if ((ColorDiff(bmp, width, 0, 0, i, j+1) > d + 10000 ||
        //   ColorDiff(bmp, width, 0, 0, i, j+2) > d + 10000
        int dd = ColorDiff(bmp, width, i, j, i-1, j);
        int dd2 = ColorDiff(bmp, width, i, j, i+1, j);
        int dd3 = ColorDiff(bmp, width, 0, 0, i+1, j);
        int d2 = ColorDiff(bmp, width, i, j, i, j+2);
        int d3 = ColorDiff(bmp, width, i, j, i, j+1);
        int d4 = ColorDiff(bmp, width, i, j, i, j-1);
        int d5 = ColorDiff(bmp, width, i, j, i, j-2);
        //*/
/*        break;
    }
    if (i >= x2*dx/9) {
      res = 1;
    }
  }
  return 0;
}//*/

wchar_t GuessChar(unsigned int *bmp, int width, int x, int y, int w, int h, int middle) {
  int hits[5][6];
  wchar_t out = 0;
  //if (y < 80) return 1;
  //if ((bmp[width*y + x] & 0xFFFFFF) == 0xFF00FF) return 1;
  //return 0;
  //h--;
  //w--;
  int rows[6];
  rows[0] = y;
  rows[5] = y+h-1;
  rows[1] = y+((1*4+3)*(h+1)+6) / 6/4 - 1;
  if (ColorDiff(bmp, width, x, rows[1], x, y) > 500) {
    rows[1] = y+((1*2+1)*(h+1)+6) / 6/2 - 1;
    if (ColorDiff(bmp, width, x, rows[1], x, y) > 500) {
      return 0;
    }
  }
  rows[4] = y+((4*2+1)*(h+1)+6) / 6/2 - 1;
  if (ColorDiff(bmp, width, x, rows[4], x, y) > 500) {
    rows[4] = y+((1*4+3)*(h+1)+6) / 6/4 - 1;
    if (ColorDiff(bmp, width, x, rows[4], x, y) > 500) {
      return 0;
    }
  }

  int cols[5] = {
    0,
    ((middle * 6 + 3) + 5)/5 / 2,
    middle,
    ((3*2+1)*w)/5/2,
    w-1
  };

  for (int n=0; n<5; n++) {
    int xr = cols[n];
    for (int m=0; m<6; m++) {
      //int y1 = m*h / 6;
      //int y2 = (m+1)*h / 6;
      int yr = -1;
      if (!m) yr = 0;
      else if (m == 5) yr = h-1;
      else yr=((m*2+1)*h)/6/2;
      //else yr = (m*2+1) * (h+1)
      /*if (y1 + 1 < y2) {
        yr = (y1+y2)/2;
      }
      else if (!m) {
        yr = y1;
      }
      else {//*/
      //}
      hits[n][m] = 0;
      int d = ColorDiff(bmp, width, x+xr, y+yr, x, y);
      if (d < 200) {
        if ((!m || !hits[n][m-1] || ColorDiff(bmp, width, x+xr, y+yr-1, x, y) < 200) &&
          (!n || !hits[n-1][m] || ColorDiff(bmp, width, x+xr-1, y+yr, x, y) < 200))
            hits[n][m] = 1;
      }
      if (!m && !hits[n][m]) return 0;
    }
  }
  int index = -1;
  for (int g=0; g<20; g++) {
    int a = g&3;
    int b = g/4;
    if (a >= 2) {
      if (!hits[2][1+b]) {
        return 0;
      }
      a++;
    }
    int v = hits[a][1+b];
    if (g == 0) {
      if (!v) {
        return 0;
      }
    }
    else if (g == 4) {
      if (v) {
        return 0;
      }
    }
    else if (g == 15) {
      if (!v) {
        return 0;
      }
    }
    else if (g == 11) {
      if (v) {
        return 0;
      }
    }
    else {
      index++;
      out += (v << index);
    }
  }
  if (out == 0xFFFF || out == 0xFFFE || out == 0xFFFE) return 0;
  return out;
}

StringInfo *LocateStrings(unsigned int *bmp, int w, int h, int &numStrings) {
  //while(1);
  int lastY = 0;
  StringInfo * strings;
  int i, j;
  {
    CharInfo *chars = 0;
    int numChars = 0;
    for (int yp=2; yp<h; yp++) {
      int len = 0;
      /*if (yp < 388) {
        for (int x=28; x<w; x++) {
          bmp[w*yp+x] = 0x00FF00;
        }
        continue;
      }//*/
      for (int xp=2; xp<w; xp++) {
        /*if (xp<27) {
          bmp[w*yp+xp] = 0x00FF00;
          continue;
        }//*/
        int d3 = ColorDiff(bmp, w, xp, yp, xp, yp-2);
        if (d3 > 100 || ColorDiff(bmp, w, xp, yp, xp, yp-1) > 100) {
          int d1 = ColorDiff(bmp, w, xp, yp, xp-1, yp);
          if (d1 < 100) {
            int d2 = ColorDiff(bmp, w, xp, yp, xp-2, yp);
            if (((!len && d2 >= 100) || (len && d2 < 100))) {
            //if ((*(unsigned int*)(bmp + 4*(w*yp+xp))&0xFFFFFF) == 0xFFFFFF) {
              len ++;
              continue;
            }
          }
          //}
        }
        if (len >= 6 && len < 80 && ColorDiff(bmp, w, xp, yp, xp-1, yp) >= 100) {
          len++;
          int dx = len;
          int x = xp-dx;
          //if (bmp[w*yp + x] == 0xFFFFFFFF) {
          //  w=w;
          //}
          int x1 = x + dx*2 / 5;
          int x2 = x + dx*3 / 5;
          int x3;
          int y, y1, y2;
          int max = yp;
          int color = 0;
          int middle = 0;
          for (x3 = (x1+x2)/2-1; x3<=(x1+x2)/2+1; x3++) {
            y = yp;
            while (y < h-2) {
              if (ColorDiff(bmp, w, x3, y+1, x3, yp) > 500) break;
              y++;
            }
            if (y == h-2) {
              max = yp + dx*100;
              break;
            }
            if (y > max) {
              middle = x3;
              color = bmp[w*(yp+1)+x3];
              max = y;
            }
            else if (y == max && x3 == (x1+x2)/2) middle = x3;
          }
          int dy = 1+max - yp;
          if ((dy >= dx*8/10 && dy <= dx * 180/100)) {
            for (y1 = 0; y1<dy; y1++) {
              if (ColorDiff(bmp, w, x-2, yp+y1, x3, yp) < 500 &&
                ColorDiff(bmp, w, x-1, yp+y1, x3, yp) < 500) {
                  break;
              }
            }
            for (y2 = 0; y2<dy; y2++) {
              if (ColorDiff(bmp, w, xp+1, yp+y2, x3, yp) < 500 &&
                ColorDiff(bmp, w, xp, yp+y2, x3, yp) < 500) {
                  break;
              }
            }
            for (x1 = 0; x1 < dx; x1++) {
              if (ColorDiff(bmp, w, xp+x1, yp+dy+1, x3, yp) < 500 &&
                ColorDiff(bmp, w, xp+x1, yp+dy, x3, yp) < 500) {
                  break;
              }
            }
            if ((y2 == y1 && y2 == dy && x1 == dx)) {
              wchar_t c = GuessChar(bmp, w, x, yp, dx, dy, middle-x);
              if (c) {
                //c = GuessChar(bmp, w, x, yp, dx, dy, middle-x);
                int over;
                for (over = numChars-1; over>=0; over--) {
                  if (chars[over].x < x+dx  && chars[over].x + chars[over].w > x &&
                    chars[over].y < yp+dy && chars[over].y + chars[over].h > yp)
                    break;
                }
                if (over < 0) {
                  /*while (1) {
                    c = GuessChar(bmp, w, x, yp, dx, dy, middle-x);
                    c=c;
                  }//*/
                  for (int q = 0; q<dx; q++) {
                    //bmp[w*yp+x+q] = 0xFFFF00FF;
                  }
                  for (int q = 0; q<dy; q++) {
                    //bmp[w*(yp+q)+x + dx/2] = 0xFFFF00FF;
                  }
                  if (c) {
                    if (numChars % 128 == 0) {
                      chars = (CharInfo*) realloc(chars, sizeof(CharInfo) * (128+numChars));
                    }
                    CharInfo *info = &chars[numChars++];
                    info->color = color;
                    info->mid = middle;
                    info->c = c;
                    info->x = x;
                    info->y = yp;
                    info->h = dy;
                    info->w = dx;
                    info->fontH = dy;
                    info->next = 0;
                    info->prev = 0;
                  }
                }
              }
            }
          }
        }
        len = 0;
      }
    }
    for (i=1; i<numChars; i++) {
      CharInfo s = chars[i];
      while (i && chars[i-1].x > s.x) {
        chars[i] = chars[i-1];
        i--;
      }
      chars[i] = s;
    }
    for (i=0; i<numChars; i++) {
      int len = 1;
      int spacing = chars[i].w/2 + 1;
      for (j = i+1; j<numChars; j++) {
        if (!chars[j].prev &&
          chars[i].y + 1 >= chars[j].y && chars[i].y - 1 <= chars[j].y &&
          chars[i].h + 1 >= chars[j].h && chars[i].h - 1 <= chars[j].h) {
            if (chars[i].x + chars[i].w <= chars[j].x &&
              chars[i].x + chars[i].w + spacing >= chars[j].x) {
                chars[i].next = &chars[j];
                chars[j].prev = &chars[i];
                break;
            }
        }
      }
    }

    numStrings = 0;
    for (i=0; i<numChars; i++) {
      if (!chars[i].prev) numStrings++;
    }
    strings = (StringInfo*) calloc(numStrings, sizeof(StringInfo));
    numStrings = 0;
    for (i=0; i<numChars; i++) {
      if (!chars[i].prev) {
        StringInfo *info = strings+numStrings;
        CharInfo *c = chars+i;
        info->chars = c;
        info->x = c->x;
        info->y = c->y;
        info->h = c->h;
        info->w = c->w;
        info->fontH = c->fontH;
        info->color = c->color;
        int len = 0;
        while (c) {
          len ++;
          c = c->next;
        }
        c = info->chars;
        info->string = (wchar_t*) malloc((len+1) * sizeof(wchar_t));
        wchar_t *s = info->string;
        while (c) {
          s++[0] = c->c;
          if (c->y < info->y) {
            info->h += info->y - c->y;
            info->y = c->y;
          }
          if (c->y + c->h > info->y + info->h) info->h = c->y + c->h - info->y;
          if (c->fontH > info->fontH) info->fontH = c->fontH;
          info->w = c->x + c->w - info->x;
          c = c->next;
        }
        *s = 0;
        numStrings++;
      }
    }
  }

  // Only do this because of my length cutoff.
  int changed = 1;
  while (changed) {
    changed = 0;
    for (i=0; i<numStrings; i++) {
      //if (wcslen(strings[i].string) < 10) continue;
      for (j=0; j<numStrings; j++) {
        if (j == i) continue;
        if (strings[i].y > strings[j].y) continue;
        if (strings[i].y + strings[i].h + 3 + strings[i].fontH/2 < strings[j].y) continue;
        if (strings[j].x < strings[i].x - 2 * strings[i].fontH || strings[j].x > strings[i].x + 2 * strings[i].fontH) continue;
        if (strings[j].fontH < strings[i].fontH - 1 || strings[j].fontH > strings[i].fontH + 1) continue;
        wchar_t *s2 = strings[j].string;
        wchar_t *e2 = s2+wcscspn(s2, L"\n");
        wchar_t *string = strings[i].string = (wchar_t*) (realloc(strings[i].string, sizeof(wchar_t) * (2 + wcslen(strings[i].string) + wcslen(s2))));
        wchar_t *s = wcsrchr(string, '\n');
        if (!s) s = string;
        else s++;
        wchar_t *e = wcschr(s, 0);
        if ((IsMajorPunctuation(s[0], -1) && IsMajorPunctuation(e[-1], -1)) ||
          (IsMajorPunctuation(s2[0], -1) && IsMajorPunctuation(e2[-1], -1))) {
            e[0] = '\n';
            e++;
        }
        wcscpy(e, strings[j].string);
        strings[i].h = strings[j].y + strings[j].h - strings[i].y;
        if (strings[j].x < strings[i].x) {
          strings[i].w += strings[i].x - strings[j].x;
          strings[i].x = strings[j].x;
        }
        if (strings[j].w + strings[j].x > strings[i].w + strings[i].x) {
          strings[i].w = strings[j].x + strings[j].w - strings[i].x;
        }
        if (strings[j].fontH > strings[i].fontH) strings[i].fontH = strings[j].fontH;
        strings[i].multiline = 1;

        free(strings[j].string);
        strings[j] = strings[--numStrings];
        if (i == numStrings) i = j;
        j = -1;
        changed = 1;
      }
    }
  }
  return strings;
}

void TransStrings(StringInfo* chars, int numChars) {
  for (int i=0; i<numChars; i++) {
    wchar_t *string = chars[i].string;
    int srcLen = wcslen(string);
    if (!HasJap(string)) {
      chars[i].trans = chars[i].string = (wchar_t*)realloc(string, sizeof(wchar_t) * (srcLen+1));
      continue;
    }
    int w = 0;
    while (w < cacheSize) {
      if (!cache[w].src) {
        break;
      }
      w++;
      continue;
      if (!wcsicmp(cache[w].src, string)) break;
      w++;
    }
    if (w == cacheSize || !cache[w].src) {
      if (w == cacheSize) {
        free(cache[cacheSize-1].src);
        cache[cacheSize-1].src = 0;
        w--;
      }
      wchar_t *trans = TranslateFullLog(string);
      if (!trans) {
        chars[i].trans = chars[i].string = (wchar_t*)realloc(string, sizeof(wchar_t) * (srcLen+1));
        continue;
      }
      wchar_t *temp = (wchar_t*) malloc(sizeof(wchar_t) * (2+srcLen + wcslen(trans)));
      cache[w].src = wcscpy(temp, string);
      cache[w].dst = wcscpy(temp + srcLen+1, trans);
      free(trans);
    }
    chars[i].string = (wchar_t*) realloc(chars[i].string, sizeof(wchar_t) * (2+srcLen + wcslen(cache[w].dst)));
    chars[i].trans = wcscpy(chars[i].string + srcLen+1, cache[w].dst);
  }
}

void DisplayStrings(HDC hDC, StringInfo* chars, int numChars, int w, int h) {
  int mode = SetBkMode(hDC, OPAQUE);
  COLORREF color = SetTextColor(hDC, RGB(255,255,255));
  COLORREF bkg = SetBkColor(hDC, RGB(0,0,255));
  int fontHeight = 1;
  HGDIOBJ hOldFont = 0;
  for (int i=0; i<numChars; i++) {
    int height = -((chars[i].fontH*7+3)/6);
    if (fontHeight != height) {
      HFONT hFont = CreateFont(height, 0, 0, 0,
        FW_REGULAR, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, FF_DONTCARE, L"Arial");
      if (hFont) {
        HGDIOBJ hFont2 = SelectObject(hDC, hFont);
        if (hFont2) {
          if (!hOldFont) {
            hOldFont = hFont2;
          }
          else {
            DeleteObject(hFont2);
          }
          fontHeight = height;
        }
      }
    }
    RECT r = {
      chars[i].x-2,
      chars[i].y-chars[i].fontH/6,
      chars[i].x + chars[i].w+5,
      chars[i].y + chars[i].h+2
    };
    if (r.right > w) r.right = w;
    if (!chars[i].multiline && r.right < w - 20) {
      r.right = w - 20;
    }
    wchar_t *string = chars[i].trans;
    DrawText(hDC, string, -1, &r, DT_WORDBREAK | DT_CALCRECT);
    int expand = 0;
    if (r.bottom >= h) {
      expand = 1;
    }
    else {
      for (int q=0; q<numChars; q++) {
        if (q == i) continue;
        if (r.left < chars[q].x + chars[q].w &&
          r.right > chars[q].x &&
          r.top < chars[q].y &&
          r.bottom + 5 > chars[q].y)
            expand = 1;
      }
    }
    if (expand) {
      r.right = w - 20;
    }
    DrawText(hDC, string, -1, &r, DT_WORDBREAK | DT_NOCLIP);
  }
  if (hOldFont) {
    HGDIOBJ hFontTemp = SelectObject(hDC, hOldFont);
    DeleteObject(hFontTemp);
  }
  SetBkColor(hDC, bkg);
  SetBkMode(hDC, mode);
  SetTextColor(hDC, color);
}

void ClearCache() {
  for (int i=0; i<cacheSize; i++) {
    if (cache[i].src)
      free(cache[i].src);
  }
  free(cache);
  cache = 0;
  cacheSize = 0;
}

void SetCacheSize(int size) {
  ClearCache();
  cacheSize = size;
  cache = (Translation*)calloc(cacheSize, sizeof(Translation));
}


