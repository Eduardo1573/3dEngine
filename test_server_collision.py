import math
import unittest

from server import (
    CHARACTER_PARTS,
    Vec3,
    bullet_box_hit_fraction,
    bullet_hits_box,
    bullet_hits_player,
    player_damage_box,
    player_damage_boxes,
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

    def test_player_damage_boxes_match_ten_box_character_shape(self):
        boxes = player_damage_boxes(
            {
                "x": 4.0,
                "y": 2.0,
                "z": 7.0,
                "yaw": 0.25,
                "pitch": -0.1,
            }
        )

        self.assertEqual(len(boxes), 10)
        self.assertEqual(len(CHARACTER_PARTS), 10)
        self.assertEqual(boxes[0].position, Vec3(4.0, 2.45, 7.0))
        self.assertEqual(boxes[0].dimensions, Vec3(0.58, 0.90, 0.32))
        self.assertEqual(boxes[1].position, Vec3(4.0, 3.16, 7.0))
        self.assertEqual(boxes[1].dimensions, Vec3(0.38, 0.38, 0.38))
        self.assertEqual(boxes[0].yaw, 0.25)
        self.assertEqual(boxes[0].pitch, 0.0)
        self.assertEqual(boxes[1].pitch, -0.1)

    def test_player_damage_box_keeps_torso_as_compatibility_box(self):
        box = player_damage_box({"x": 0.0, "y": 1.0, "z": 0.0})

        self.assertEqual(box.position, Vec3(0.0, 1.45, 0.0))
        self.assertEqual(box.dimensions, Vec3(0.58, 0.90, 0.32))

    def test_torso_leg_joint_is_player_position(self):
        boxes = player_damage_boxes({"x": 0.0, "y": 1.0, "z": 0.0, "yaw": 0.0})
        torso = boxes[0]
        left_upper_leg = boxes[6]
        right_upper_leg = boxes[8]

        self.assertAlmostEqual(torso.position.y - torso.dimensions.y / 2.0, 1.0)
        self.assertAlmostEqual(left_upper_leg.position.y + left_upper_leg.dimensions.y / 2.0, 1.0)
        self.assertAlmostEqual(right_upper_leg.position.y + right_upper_leg.dimensions.y / 2.0, 1.0)

    def test_bullet_hits_player_returns_first_hit_fraction(self):
        hit = bullet_hits_player(
            Vec3(0.0, 1.45, -2.0),
            Vec3(0.0, 1.45, 2.0),
            {"x": 0.0, "y": 1.0, "z": 0.0, "yaw": 0.0, "pitch": 0.0},
        )

        self.assertIsNotNone(hit)
        self.assertAlmostEqual(hit, 0.46)


if __name__ == "__main__":
    unittest.main()
