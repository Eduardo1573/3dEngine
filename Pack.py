import math

def Cube(x, y, z, n):
    return [[ [[x, y, z+n], [x+n, y, z+n], [x+n, y, z], [x, y, z],
    [x, y+n, z+n], [x+n, y+n, z+n], [x+n, y+n, z], [x, y+n, z]],

    [[0, 1, 2], [0, 2, 3], [0, 1, 5], [0, 4, 5],
    [1, 2, 5], [2, 5, 6], [2, 3, 7], [2, 6, 7],
    [0, 3, 7], [0, 4, 7], [4, 5, 7], [5, 6, 7]],

    [200, 200, 200] ]]

def Parallelepiped(x, y, z, length, height, width, red, green, blue):
    return [[ [[x, y, z+width], [x+length, y, z+width], [x+length, y, z], [x, y, z],
    [x, y+height, z+width], [x+length, y+height, z+width], [x+length, y+height, z], [x, y+height, z]],

    [[0, 1, 2], [0, 2, 3], [0, 1, 5], [0, 4, 5],
    [1, 2, 5], [2, 5, 6], [2, 3, 7], [2, 6, 7],
    [0, 3, 7], [0, 4, 7], [4, 5, 7], [5, 6, 7]],

    [[red, green, blue]]*12 ]]

def Plane(n):
    points = [ [-n, 0, i] for i in range(-n, n+1) ] + [ [n, 0, i] for i in range(-n, n+1) ]
    points += [ [i, 0, -n] for i in range(-n, n+1) ] + [ [i, 0, n] for i in range(-n, n+1) ]
    total = n*2+1
    pairs = [ [i, i + total] for i in range(0, total)] + [ [i + 2*total, i + 3*total] for i in range(0, total)]
    return [points, pairs]

GPT_Obj_1 = [
    [
        [-3, 3, 1.618],  [-3, 1, 1.618],  [-3, 3, -1.618],  [-3, 1, -1.618],
        [-1.372, 2, 1],  [-4.618, 2, 1],  [-1.372, 2, -1],  [-4.618, 2, -1],
        [-2, 3.618, 0],  [-4, 3.618, 0],  [-2, 0.372, 0],  [-4, 0.372, 0]
    ],

    [
        [0, 1], [0, 4], [0, 5], [0, 8], [0, 9], [1, 4], [1, 5], [1, 10],
        [1, 11], [2, 3], [2, 6], [2, 7], [2, 8], [2, 9], [3, 6], [3, 7],
        [3, 10], [3, 11], [4, 6], [4, 8], [4, 10], [5, 7], [5, 9], [5, 11],
        [6, 8], [6, 10], [7, 9], [7, 11], [8, 9], [10, 11]
    ]
]

def generate_tree(layers=5, points_per_layer=20, height=10, base_radius=5, trunk_height=2, trunk_radius=1):
    points = []  # Список вершин
    sides = []   # Список рёбер

    # Генерация ствола
    for i in range(2):  # Две точки для основания и вершины ствола
        z = i * trunk_height
        points.append([0, 0, z])

    # Соединение ствола
    trunk_base_idx = len(points) - 2
    trunk_top_idx = len(points) - 1
    sides.append([trunk_base_idx, trunk_top_idx])

    # Генерация слоёв ветвей
    for layer in range(layers):
        layer_height = trunk_height + (height - trunk_height) * (layer / layers)
        layer_radius = base_radius * (1 - layer / layers)
        angle_step = 2 * math.pi / points_per_layer

        layer_points_start_idx = len(points)
        for i in range(points_per_layer):
            angle = i * angle_step
            x = layer_radius * math.cos(angle)
            y = layer_radius * math.sin(angle)
            points.append([x, y, layer_height])

            # Соединение точек в круге
            if i > 0:
                sides.append([layer_points_start_idx + i - 1, layer_points_start_idx + i])

        # Замыкаем круг
        sides.append([layer_points_start_idx, layer_points_start_idx + points_per_layer - 1])

        # Соединяем текущий слой с предыдущим
        if layer > 0:
            prev_layer_points_start_idx = layer_points_start_idx - points_per_layer
            for i in range(points_per_layer):
                sides.append([prev_layer_points_start_idx + i, layer_points_start_idx + i])

    # Соединяем верхушку
    top_idx = len(points)
    points.append([0, 0, height])
    for i in range(points_per_layer):
        sides.append([top_idx, len(points) - points_per_layer - 1 + i])

    return [points, sides]

GPT_Obj_2 = generate_tree(layers=7, points_per_layer=30, height=15, base_radius=5, trunk_height=3, trunk_radius=1)

Object_Masha = [
    # Points (координаты вершин)
    [
        [0, 0, 0],       # Основание первой опоры
        [4, 0, 0],       # Основание второй опоры
        [4, 4, 0],       # Основание третьей опоры
        [0, 4, 0],       # Основание четвёртой опоры
        [1, 1, 10],      # Верхняя точка первой опоры
        [3, 1, 10],      # Верхняя точка второй опоры
        [3, 3, 10],      # Верхняя точка третьей опоры
        [1, 3, 10],      # Верхняя точка четвёртой опоры
        [1.5, 1.5, 20],  # Нижняя платформа
        [2.5, 1.5, 20],  # Нижняя платформа
        [2.5, 2.5, 20],  # Нижняя платформа
        [1.5, 2.5, 20],  # Нижняя платформа
        [2, 2, 30],      # Верхняя платформа
        [2, 2, 50]       # Шпиль
    ],
    # Sides (соединения точек)
    [
        [0, 1], [1, 2], [2, 3], [3, 0],  # Основание
        [0, 4], [1, 5], [2, 6], [3, 7],  # Вертикальные рёбра опор
        [4, 5], [5, 6], [6, 7], [7, 4],  # Верх опор
        [4, 8], [5, 9], [6, 10], [7, 11], # Соединение опор с нижней платформой
        [8, 9], [9, 10], [10, 11], [11, 8], # Нижняя платформа
        [8, 12], [9, 12], [10, 12], [11, 12], # Соединение платформ
        [12, 13]  # Шпиль
    ]
]

if __name__ == '__main__':
    pass
