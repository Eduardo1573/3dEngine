import argparse
import os

import pygame
from math import sin, cos, sqrt, pi, tan, acos
from time import perf_counter, sleep
from Multiplayer import MultiplayerClient
from Pack import *
from ObjUnpacker import ObjConv
# from main import plane


def parse_color(value):
    try:
        channels = [int(channel.strip()) for channel in value.split(",")]
    except ValueError:
        channels = [100, 255, 150]

    if len(channels) != 3:
        channels = [100, 255, 150]

    return tuple(max(0, min(255, channel)) for channel in channels)


def parse_args():
    parser = argparse.ArgumentParser(description="Run the 3D game client")
    parser.add_argument("host", nargs="?", default=None)
    parser.add_argument("--host", dest="host_option", default=None)
    parser.add_argument("--port", type=int, default=5555)
    parser.add_argument("--offline", action="store_true")
    parser.add_argument("--name", default="Player")
    parser.add_argument("--color", default="100,255,150")
    args = parser.parse_args()
    args.host = args.host_option or args.host or "127.0.0.1"
    args.color = parse_color(args.color)
    return args


PLAYER_POLYGONS = [
    [0, 1, 2], [0, 2, 3],
    [4, 6, 5], [4, 7, 6],
    [0, 4, 5], [0, 5, 1],
    [1, 5, 6], [1, 6, 2],
    [2, 6, 7], [2, 7, 3],
    [3, 7, 4], [3, 4, 0],
]


def make_player_object(player):
    x = float(player.get("x", 0))
    y = float(player.get("y", 1))
    z = float(player.get("z", -10))
    color = player.get("color", [100, 255, 150])
    color = tuple(max(0, min(255, int(channel))) for channel in color[:3])

    half_width = 0.25
    bottom = y - 1.0
    top = y + 0.25

    points = [
        [x - half_width, bottom, z + half_width],
        [x + half_width, bottom, z + half_width],
        [x + half_width, bottom, z - half_width],
        [x - half_width, bottom, z - half_width],
        [x - half_width, top, z + half_width],
        [x + half_width, top, z + half_width],
        [x + half_width, top, z - half_width],
        [x - half_width, top, z - half_width],
    ]

    colors = [color for _ in PLAYER_POLYGONS]
    return [points, PLAYER_POLYGONS, colors]


BASE_DIR = os.path.dirname(os.path.abspath(__file__))
args = parse_args()
multiplayer = MultiplayerClient(
    args.host,
    args.port,
    args.name,
    args.color,
    enabled=not args.offline,
)
multiplayer.connect()

fps = 60
lx, ly = 2560, 1600
speed = 0.1

ToSunVector = [-1, 1, -1]

pygame.init()
pygame.display.set_caption("lol its actually 3d")

fov = pi / 2
l = 1
real_screen_width = l * tan(fov / 2) * 2
real_screen_height = real_screen_width * ly/lx

display = pygame.display.set_mode((lx, ly), pygame.FULLSCREEN)
running = True
font = pygame.font.SysFont('kongtext.ttf',  50)

tiles = 5
plane = Plane(tiles)

pygame.mouse.set_visible(False)
pygame.mouse.set_pos(lx / 2, ly / 2)
mouse_x, mouse_y = pygame.mouse.get_pos()
set_once = False

Objects = []

# Objects += Parallelepiped(2.6, 0, 2.6, 0.8, 0.2, 0.8)
# Objects += Parallelepiped(2.7, 0.2, 2.7, 0.6, 3.6, 0.6)
# Objects += Parallelepiped(2.6, 3.8, 2.6, 0.8, 0.2, 0.8)
# Objects += Cube(-1, 0, -1, 2)
Objects += ObjConv(os.path.join(BASE_DIR, 'sphere.obj'), ToSunVector)

# Objects += [GPT_Obj_1]

#  (170, 57, 57), (212, 106, 106), (123, 159, 53), (165, 198, 99), (34, 102, 102), (64, 127, 127)



yaw = 0.0001
pitch = 0.0001
X = 0
Y = 1
Z = -10
end, start = 0, 0

brightness = [i/100 for i in range(100, 30, -1)] + [i/100 for i in range(30, 100)]
frame = 0

l = 1

half_pi = pi / 2

Tutor_Done = False

for event in pygame.event.get():
    if event.type == pygame.QUIT:
        running = False

# display.fill((55, 55, 55))

Text = '''Movement: W, A, S, D
Up/Down: Spase, Shift
Press "C" to continue, "esc" to escape'''
Lines = Text.split('\n')
T = -len(Lines) / 2 * 60
# for  i, line in enumerate(Lines):
#     text = font.render(line, True, (255, 255, 255))
#     textRect = text.get_rect()
#     textRect.center = (lx / 2, ly / 2 + i*60 + T)
#     display.blit(text, textRect)

while not Tutor_Done:
    pygame.time.delay(int(1000 / fps))

    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False

    key = pygame.key.get_pressed()

    if key[pygame.K_c]:
        Tutor_Done = True

    frame += 1
    frame %= 140

    display.fill((0, 0, 0))

    for i, line in enumerate(Lines):
        text = font.render(line, True, (int(255 * brightness[frame]), int(255 * brightness[frame]), int(255 * brightness[frame])))
        textRect = text.get_rect()
        textRect.center = (lx / 2, ly / 2 + i * 60 + T)
        display.blit(text, textRect)

    if key[pygame.K_ESCAPE]:
        running = False
        Tutor_Done = True
    else:
        pygame.display.update()



while running:
    pygame.time.delay(max(1, int(1000 / fps - (end-start)*1000)))

    start = perf_counter()

    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False

    display.fill((0, 200, 255))

    old_m_x, old_m_y = mouse_x, mouse_y
    mouse_x, mouse_y = pygame.mouse.get_pos()
    if mouse_x in [0, lx - 1]:
        pygame.mouse.set_pos(lx / 2, ly / 2)
    elif mouse_y in [0, ly - 1]:
        pygame.mouse.set_pos(lx / 2, ly / 2)
    d_mouse_x, d_mouse_y = mouse_x - old_m_x, mouse_y - old_m_y

    yaw += d_mouse_x / 100
    pitch -= d_mouse_y / 100

    if not set_once:
        yaw, pitch = 0.0001, 0.0001
        set_once = True

    if abs(pitch) >= half_pi:
        pitch *= pi / 2.00000001 / abs(pitch)

    sin_yaw, cos_yaw = sin(yaw), cos(yaw)
    sin_pitch, cos_pitch = sin(pitch), cos(pitch)

    key = pygame.key.get_pressed()

    if key[pygame.K_d]:
        X += speed * cos_yaw
        Z += -speed * sin_yaw
    elif key[pygame.K_a]:
        X += -speed * cos_yaw
        Z += speed * sin_yaw

    if key[pygame.K_w]:
        X += speed * sin_yaw
        Z += speed * cos_yaw
    elif key[pygame.K_s]:
        X -= speed * sin_yaw
        Z -= speed * cos_yaw

    if key[pygame.K_SPACE]:
        Y += speed
    elif key[pygame.K_LSHIFT]:
        Y -= speed

    if key[pygame.K_r]:
        yaw = 0.0001
        pitch = 0.0001
        X = 0
        Y = 1
        Z = -10

    multiplayer.send_state(X, Y, Z, yaw, pitch)

    x_ = l * sin_yaw * cos_pitch  # a_ - vec to screen in 3D, a__ - screen center, ABCD - plane params
    y_ = l * sin_pitch
    z_ = l * cos_yaw * cos_pitch

    x__ = X + x_
    y__ = Y + y_
    z__ = Z + z_

    A = x_
    B = y_
    C = z_
    D = -(x_ * x__ + y_ * y__ + z_ * z__)

    Polygons_with_dist = []

    frame_objects = Objects + [make_player_object(player) for player in multiplayer.get_other_players()]

    for Object in frame_objects:# + [plane]:
        Points, Polygons, Colors = Object
        D_Points = []

        for point in Points:
            x0, y0, z0 = point  # a0 - rendering point, A - player pos, _a - vec to point, ap - interception

            _x = x0 - X
            _y = y0 - Y
            _z = z0 - Z

            dist_to = sqrt(_x ** 2 + _y ** 2 + _z ** 2)

            cos_phi = (x_ * _x + y_ * _y + z_ * _z) / l / dist_to
            at_the_back = True if cos_phi < 0 else False

            t = (A * X + B * Y + C * Z + D) / (A * _x + B * _y + C * _z)  # parameter
            xp = X - t * _x
            yp = Y - t * _y
            zp = Z - t * _z

            y_height = real_screen_height * cos_pitch
            upper_edge = y__ + 0.5 * y_height
            y_real_pos = upper_edge - yp
            y00 = ly * y_real_pos / y_height

            y_delta = yp - y__
            xz_delta = y_delta * -sin_pitch / cos_pitch
            x_delta = xz_delta * sin_yaw

            x_width = real_screen_width * cos_yaw
            left_edge = x__ - 0.5 * x_width
            x_real_pos = xp - left_edge - x_delta
            x00 = lx * x_real_pos / x_width

            D_Points += [(x00, y00, at_the_back, dist_to)]


        for i, polygon in enumerate(Polygons):
            polygon_with_dist = (D_Points[polygon[0]][3]+D_Points[polygon[1]][3]+D_Points[polygon[2]][3])/3, [D_Points[polygon[0]], D_Points[polygon[1]], D_Points[polygon[2]]], Colors[i]
            # print(polygon_with_dist)
            Polygons_with_dist += [polygon_with_dist]

    Polygons_with_dist.sort(reverse=True)

    for polygon in Polygons_with_dist:
        # for i in polygon: print(i)
        three_points_pairs = [[polygon[1][0][0], polygon[1][0][1]],
                             [polygon[1][1][0], polygon[1][1][1]],
                             [polygon[1][2][0], polygon[1][2][1]]]

        all_x = [-lx <= x[0] <= 2*lx for x in three_points_pairs]
        all_y = [-ly <= y[1] <= 2*ly for y in three_points_pairs]
        if all_x == all_y == [True,True,True] and not polygon[1][0][2] and not polygon[1][1][2] and not polygon[1][2][2]:
            pygame.draw.polygon(display, polygon[2], (three_points_pairs))
            pygame.draw.polygon(display, (0, 0, 0), (three_points_pairs), 1)

        # if not polygon[1][0][2] and not polygon[1][1][2] and not polygon[1][2][2]:
        #     pygame.draw.polygon(display, polygon[2], (three_points_pairs))
        #     pygame.draw.polygon(display, (0, 0, 0), (three_points_pairs), 1)
        # elif polygon[1][0][2] and polygon[1][1][2] and polygon[1][2][2]:
        #     pass
        # elif polygon[1][0][2] and polygon[1][1][2]:
                


        # for side in Polygons:
        #     if not D_Points[side[0]][2] and not D_Points[side[1]][2]:
        #         pygame.draw.line(display, (255, 255, 255), D_Points[side[0]][:2], D_Points[side[1]][:2], 1)
        #     elif D_Points[side[0]][2] and D_Points[side[1]][2]:
        #         pass
        #     elif D_Points[side[0]][2]:
        #         xa, ya = D_Points[side[0]][:2]
        #         xb, yb = D_Points[side[1]][:2]
        #         end_x = 2 * xb - xa
        #         end_y = 2 * yb - ya
        #         while 0 < end_x < lx and 0 < end_y < ly:
        #             end_x += xb - xa
        #             end_y += yb - ya
        #         pygame.draw.line(display, (255, 255, 255), (end_x, end_y), D_Points[side[1]][:2], 1)
        #     elif D_Points[side[1]][2]:
        #         xa, ya = D_Points[side[1]][:2]
        #         xb, yb = D_Points[side[0]][:2]
        #         end_x = 2 * xb - xa
        #         end_y = 2 * yb - ya
        #         while 0 < end_x < lx and 0 < end_y < ly:
        #             end_x += xb - xa
        #             end_y += yb - ya
        #         pygame.draw.line(display, (255, 255, 255), D_Points[side[0]][:2], (end_x, end_y), 1)

    #         sleep(0.2)
    #         pygame.display.update()

    end = perf_counter()

    FOV = font.render(f'{int(fov / pi * 180)}', True, (255, 255, 255))
    textRect = FOV.get_rect()
    textRect.center = (300, 100)
    display.blit(FOV, textRect)

    XYZ = font.render(f'{int(X // 1)}, {int(Y // 1)}, {int(Z // 1)}', True, (255, 255, 255))
    textRect = XYZ.get_rect()
    textRect.center = (300, 150)
    display.blit(XYZ, textRect)

    FRAMETIME = font.render(f'{(end-start)//0.0001/10} ms', True, (255, 255, 255))
    textRect = FRAMETIME.get_rect()
    textRect.center = (300, 200)
    display.blit(FRAMETIME, textRect)

    NET = font.render(multiplayer.hud_text(), True, (255, 255, 255))
    textRect = NET.get_rect()
    textRect.center = (300, 250)
    display.blit(NET, textRect)

    if key[pygame.K_UP]:
        fov = (fov / pi * 180 + 1) * pi / 180
        real_screen_width = l * tan(fov / 2) * 2
        real_screen_height = real_screen_width * ly / lx
    elif key[pygame.K_DOWN]:
        fov = (fov / pi * 180 - 1) * pi / 180
        real_screen_width = l * tan(fov / 2) * 2
        real_screen_height = real_screen_width * ly / lx

    if key[pygame.K_RIGHT]:
        tiles += 1
        plane = Plane(tiles)
    elif key[pygame.K_LEFT]:
        tiles -= 1 if tiles >= 1 else 0
        plane = Plane(tiles)

    pygame.display.update()

    if key[pygame.K_ESCAPE]:
        pygame.quit()
        running = False

multiplayer.close()
if pygame.get_init():
    pygame.quit()
