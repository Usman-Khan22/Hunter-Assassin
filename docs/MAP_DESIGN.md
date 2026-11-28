# Facility map design

The facility is 50 columns by 35 rows. Each tile is 48 world units, giving a 2400 by 1680 world. The window is 1280 by 720 at 1.5× camera zoom. Coordinates in this guide are zero-based (column, row).

## Files and tile legend

- [assets/maps/facility.txt](../assets/maps/facility.txt) is the architectural grid. `W` is a wall, `.` is room floor, and `,` is corridor floor.
- [assets/maps/objects.txt](../assets/maps/objects.txt) stores one `type tileX tileY rotation` record per line. A line starting with `#` is a comment. Rotation is in degrees, positive clockwise on screen. Zero faces right for actors. Tile (4, 4) centers at world (216, 216).
- [assets/png](../assets/png) contains the exact-size background and seven reusable sprites. For F5, keep the assets directory beside game.exe. [assets/mockups](../assets/mockups) contains previews, and [assets/src](../assets/src) contains the original generators.
- Crates mark whole cells solid in a separate occupancy array. Doors start closed and occupy both tiles until unlocked. Terminals are interactive. The final exit stays solid until a successful E interaction completes the mission. The outer grid boundary always blocks movement.

## Rooms and landmarks

- **A, cells** starts the player at (4, 4). **B, storage** holds cover crates. **C, servers** holds a terminal at (28, 2). **D, security** holds a terminal at (46, 2), an access card at (47, 7), and doors at (42, 9) and (43, 9).
- **E, lab** has a partition and a southbound vent. **F, central hall** has pillars and patrols. **G, control** has a partition, a terminal at (46, 19), and doors at (35, 18) and (35, 19).
- **H, cargo** is a crate-filled cover area. **I, maintenance** holds an access card at (25, 32). **J, reactor** has a core wall and connects to **K, exit lobby** through doors at (40, 29) and (40, 30). The lobby's north doors are at (45, 25) and (46, 25), and its east exit is at (49, 29) and (49, 30).
- The north and south corridors run along rows 10–11 and 23–24. The two vertical connectors sit at columns 12–13 and 33–34.

## Guard patrols

Each row starts at the spawn from objects.txt and loops back there. BFS connects consecutive waypoints. The rows follow guard-record order in the placement file.

1. North corridor — (24, 10) → (20, 10) → (20, 11) → (25, 11).
2. South corridor west — (8, 23) → (12, 23) → (12, 24) → (6, 24).
3. East connector — (33, 17) → (33, 21) → (34, 21) → (34, 13).
4. Central hall — (22, 17) → (24, 17) → (24, 14) → (17, 14).
5. Security office — (43, 5) → (39, 5) → (39, 2) → (44, 2).
6. Reactor — (37, 30) → (37, 26) → (38, 26) → (38, 32).
7. Cargo bay — (5, 30) → (7, 30) → (7, 32) → (5, 32).

## Access and progression

Walk near a card to collect it, then press E near a door to use your cards. Cards stay in your inventory. The HUD counts both cards and shows access feedback.

- Door 1, Security entrance, (42, 9) and (43, 9), requires Card B.
- Door 2, Control entrance, (35, 18) and (35, 19), requires Card A.
- Door 3, north Exit Lobby entrance, (45, 25) and (46, 25), requires both.
- Door 4, Reactor to Exit Lobby, (40, 29) and (40, 30), requires both.
- Door 5, final exit, (49, 29) and (49, 30), requires both cards, the lockdown override, remaining health, and time on the countdown. Press E from within reach to escape.

With all doors closed and 19 crates in place, 1,194 tiles are reachable from spawn, including both cards, all three terminals, and every guard spawn. Security is reachable from Servers via the row-4 passage. Maintenance is accessible from the south corridor, and Control has north and south entrances. The two ordinary Exit Lobby doors provide alternate entrances once both cards are collected. F3 shows blocked tiles and guard routes.

## Security terminals

- Servers, (28, 2), reveals the Nexus-7 station ID, 07.
- Security, (46, 2), reveals the experiment number, 42.
- Control, (46, 19), requires both access cards. Combine the first and last pairs of digits to submit the four-digit override. The clues can be reread in either order.

Press E near a terminal and Escape to close it. Guard patrols and combat continue while you read or type. The six-minute timer continues at terminals and after the override.

## Complete mission route

1. Start in Cells with 06:00 on the clock. Reach Security via the Servers passage and collect Card A at (47, 7). Collect Card B in Maintenance at (25, 32) via the south corridor. Either card can be collected first.
2. Read the Servers and Security logs for the two code halves. With both cards, use Control at (46, 19) and submit the four-digit code.
3. Open either ordinary Exit Lobby door with both cards. Approach either final-exit tile from inside the lobby and press E before time or health runs out. The mission ends on the interaction tile.
4. On the results screen, press R for a fresh mission or Escape to quit.

## Reinforcement entry points

The scheduler tries these entry points in order. Each patrol loops between adjacent floor tiles.

1. Security office east, (47, 3) to (47, 4).
2. West end of the south corridor, (2, 24) to (3, 24).
3. North corridor near the east connector, (32, 11) to (31, 11).

Each entry must be outside the camera view, at least three tile widths from the player, and clear of living guards and authored objects. A queued reinforcement tries again up to three times at one-second intervals if all entries are unsafe. It investigates the tile where detection started if BFS can reach it. Added guards use the existing melee sprites and appear on the minimap only with F3.

## Laser guard placements

The Central Hall guard (22, 17) and Security Office guard (43, 5) use the laser type. Their original spawns and patrol routes are unchanged. The other five initial guards and all reinforcements are melee. Use pillars, walls, crates, or closed doors to break the firing line. The aiming line remains fixed during the charge, so a sideways dodge can avoid it even in the open.
