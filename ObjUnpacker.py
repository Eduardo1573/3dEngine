from math import sqrt

def ObjConv(file, ToSunVector):
    object = open(file)
    v = []
    p = []
    vn = []
    vnn = []
    colors = []
    for line in object:
        line = line[0:-1].split(' ')
        if line[0] == 'vn':
            vn += [list(map(float, line[1:]))]
    object = open(file)
    for line in object:
        line = line[0:-1].split(' ')
        if line[0] == 'v':
            v += [list(map(lambda x: float(x), line[1:]))]
        elif line[0] == 'f':
            if line[1].count('/') == 2:
                p += [[int(line[1].split('/')[0]) - 1, int(line[2].split('/')[0]) - 1, int(line[3].split('/')[0]) - 1]]
                vnX, vnY, vnZ = vn[int(line[1].split('/')[2]) - 1]
                koef = (-vnX + vnY - vnZ) / sqrt(3)
                if koef < 0: koef = 0
                colors += [[int(200)*(0.2+0.8*koef)]*3]
            else:
                p += [[int(line[1].split('/')[0]) - 1, int(line[2].split('/')[0]) - 1, int(line[3].split('/')[0]) - 1]]
                koef = 1
                colors += [[int(200) * (0.2 + 0.8 * koef)] * 3]
    return [[v, p, colors]]

if __name__ == '__main__':
    print(ObjConv('cube.obj'))
