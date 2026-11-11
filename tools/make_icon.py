"""The program's icon (its own pixel art, not from the game): a gold fighter, twin shots, on a dark rounded
square. Writes ports/sdl2/icon/donpachi.ico (16-256 pixels, for the .exe), ports/sdl2/icon/donpachi.png (256) and
ports/sdl2/icon_pixels.h (32 x 32, the window's icon). Run: python tools/make_icon.py (needs Pillow)

license:BSD-3-Clause
"""
import os
from PIL import Image

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'ports', 'sdl2')
N=32
img=Image.new('RGBA',(N,N),(0,0,0,0))
px=img.load()
# background: rounded square, navy gradient, gold rim
def inside(x,y,r=5):
    cx=min(max(x,r),N-1-r); cy=min(max(y,r),N-1-r)
    return (x-cx)**2+(y-cy)**2 <= r*r+r*0.6
for y in range(N):
    for x in range(N):
        if inside(x,y):
            t=y/(N-1)
            c=(int(30-18*t),int(30-18*t),int(70-42*t),255)
            px[x,y]=c
for y in range(N):
    for x in range(N):
        if px[x,y][3] and any(not(0<=x+dx<N and 0<=y+dy<N) or px[x+dx,y+dy][3]==0 for dx,dy in((1,0),(-1,0),(0,1),(0,-1))):
            px[x,y]=(200,150,40,255)
# stars
for (x,y) in [(6,6),(25,9),(8,22),(27,24),(4,15)]:
    px[x,y]=(150,150,190,255)
GOLD=(246,196,64,255); LIGHT=(255,236,150,255); DARK=(176,112,24,255); DEEP=(110,62,14,255)
CYAN=(90,220,255,255); WHITE=(255,255,255,255); ORANGE=(255,140,30,255); RED=(230,60,30,255)
# ship (left half, mirrored): rows -> (from_x, to_x) for left half, x in 0..15 meaning column; centre columns 15,16
ship={
 5:(15,15), 6:(15,15), 7:(14,15), 8:(14,15), 9:(14,15), 10:(13,15), 11:(13,15), 12:(13,15),
 13:(12,15), 14:(10,15), 15:(8,15), 16:(6,15), 17:(5,15), 18:(5,15), 19:(6,15), 20:(9,15),
 21:(11,15), 22:(11,15), 23:(10,15), 24:(9,15), 25:(10,15),
}
for y,(a,b) in ship.items():
    for x in range(a,b+1):
        for xx in (x,31-x):
            # shading: left half lit, right half darker; edges dark
            edge = x==a
            c = DARK if edge else (GOLD if xx<16 else (226,170,48,255))
            px[xx,y]=c
# highlights: spine light
for y in range(6,22): px[15,y]=LIGHT
# wing leading edge highlight
for (x,y) in [(6,16),(7,16),(8,15),(9,15),(10,14),(11,14)]:
    px[x,y]=LIGHT
# wing tips darker
for (x,y) in [(5,17),(5,18),(26,17),(26,18)]: px[x,y]=DEEP
# cockpit
for (x,y) in [(15,10),(16,10),(15,11),(16,11),(15,12),(16,12)]: px[x,y]=CYAN
px[15,10]=WHITE
# engine flames
for (x,y,c) in [(12,26,ORANGE),(13,26,WHITE),(14,26,LIGHT),(12,27,RED),(13,27,LIGHT),(14,27,ORANGE),(13,28,ORANGE),(14,28,RED),(13,29,RED),
                (17,26,LIGHT),(18,26,WHITE),(19,26,ORANGE),(17,27,ORANGE),(18,27,LIGHT),(19,27,RED),(17,28,RED),(18,28,ORANGE),(18,29,RED)]:
    px[x,y]=c
# twin shots above the nose
for y in (1,2): pass
for x in (11,20):
    for y in range(2,9):
        px[x,y]=CYAN if y%3 else WHITE
# red stripes on the wings
for (x,y) in [(8,17),(9,17),(10,17),(9,16),(10,16),(11,16),(11,15),(12,15)]:
    px[x,y]=RED; px[31-x,y]=(196,48,24,255)
big=img.resize((256,256),Image.NEAREST)
sizes={256:big,128:img.resize((128,128),Image.NEAREST),64:img.resize((64,64),Image.NEAREST),32:img,
       48:big.resize((48,48),Image.LANCZOS),24:big.resize((24,24),Image.LANCZOS),16:big.resize((16,16),Image.LANCZOS)}
big.save(os.path.join(OUT, 'icon', 'donpachi.ico'),format='ICO',sizes=[(s,s) for s in sorted(sizes)],append_images=[sizes[s] for s in sorted(sizes) if s!=256])
big.save(os.path.join(OUT, 'icon', 'donpachi.png'))
# the window's icon for SDL: 32 x 32 RGBA, as C
with open(os.path.join(OUT, 'icon_pixels.h'), 'w', newline='\n') as f:
    f.write('/* The window icon: 32 x 32, 0xAARRGGBB (made by tools/make_icon.py; the program\'s own art) */\n')
    f.write('static const unsigned int icon_pixels[32 * 32] = {\n')
    data=[img.getpixel((x, y)) for y in range(N) for x in range(N)]
    for r in range(32):
        f.write('\t'+', '.join('0x%02X%02X%02X%02Xu'%(a,rr,g,b) for (rr,g,b,a) in data[r*32:r*32+32])+',\n')
    f.write('};\n')
