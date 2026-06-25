import math
import unittest

from server import (
    PLAYER_VISUAL_DEPTH,
    PLAYER_VISUAL_HEIGHT,
    PLAYER_VISUAL_WIDTH,
    Vec3,
    bullet_box_hit_fraction,
    bullet_hits_box,
    bullet_hits_player,
    player_damage_box,
)


class ServerCollisionTests(unittest.TestCase):
    def test_bullet_segment_hits_axis_aligned_box(self):
        hit = bullet_box_hit_fraction(
            Vec3(-2.0, 0.0, 0.0),
            Vec3(2.0, 0.0, 0.0),
            Vec3(0.0, 0.0, 0.0),
            Vec3(1.0, 1.0, 1.0),
        )

        self.assertIsNotNone(hit)
        self.assertAlmostEqual(hit, 0.375)

    def test_bullet_segment_misses_axis_aligned_box(self):
        self.assertFalse(
            bullet_hits_box(
                Vec3(-2.0, 2.0, 0.0),
                Vec3(2.0, 2.0, 0.0),
                Vec3(0.0, 0.0, 0.0),
                Vec3(1.0, 1.0, 1.0),
            )
        )

    def test_bullet_segment_hits_yawed_box_using_same_visual_dimensions(self):
        self.assertTrue(
            bullet_hits_box(
                Vec3(-1.0, 0.0, -1.0),
                Vec3(1.0, 0.0, 1.0),
                Vec3(0.0, 0.0, 0.0),
                Vec3(0.5, 1.0, 2.0),
                yaw=math.pi / 4.0,
            )
        )

    def test_player_damage_box_matches_visual_character_shape(self):
        box = player_damage_box(
            {
                "x": 4.0,
                "y": 2.0,
                "z": 7.0,
                "yaw": 0.25,
                "pitch": -0.1,
            }
        )

        self.assertEqual(box.position, Vec3(4.0, 1.625, 7.0))
        self.assertEqual(
            box.dimensions,
            Vec3(PLAYER_VISUAL_WIDTH, PLAYER_VISUAL_HEIGHT, PLAYER_VISUAL_DEPTH),
        )
        self.assertEqual(box.yaw, 0.25)
        self.assertEqual(box.pitch, -0.1)

    def test_bullet_hits_player_returns_first_hit_fraction(self):
        hit = bullet_hits_player(
            Vec3(0.0, 1.0, -2.0),
            Vec3(0.0, 1.0, 2.0),
            {"x": 0.0, "y": 1.0, "z": 0.0, "yaw": 0.0, "pitch": 0.0},
        )

        self.assertIsNotNone(hit)
        self.assertAlmostEqual(hit, 0.4375)


if __name__ == "__main__":
    unittest.main()
