import pygame, sys
pygame.init()

display = pygame.display.set_mode((2560, 1600), pygame.FULLSCREEN)
running = True

pixel_x, pixel_y = 2468*3, 1251
colors = [(200, 0, 0), (0, 155, 0), (0, 0, 255)]

while running:

    pygame.time.delay(0)

    display.fill((0, 0, 0))

    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False

    # mouse = pygame.mouse.get_pos()

    key = pygame.key.get_pressed()

    if key[pygame.K_UP]:
        pixel_y -= 1
    if key[pygame.K_DOWN]:
        pixel_y += 1
    if key[pygame.K_LEFT]:
        pixel_x -= 1
    if key[pygame.K_RIGHT]:
        pixel_x += 1

    if key[pygame.K_ESCAPE]:
        running = False
        pygame.quit()
        sys.exit()

    color = colors[pixel_x%3]
    pygame.draw.rect(display, color, [pixel_x//3, pixel_y, 1, 1])

    pygame.display.flip()

    # print(mouse)