# Documentation

## Maps

Maps work like this (assuming the map is of width 4):

```text
00  01  02  03
  04  05  06  07
08  09  10  11
  12  13  14  15
16  17  18  19
```

Which means that to move horizontally, you can add/sub 1 to the cell id.

Here are the directions you can take:

- North/South: `cell_id -/+ WIDTH`
- West/East: `cell_id -/+ 1`
- NE/SE and NW/SW: combination of those rules.

Paths are found using A* in game.

## Moving

To move, you need to send a `MapMovementRequest`. The server should send a `MapMovementEvent`.

You have to compute the time it would take to go to the destination cell, then send
a `MapConfirmRequest`.

## Changing maps

You need to get to the correct cell and have the correct orientation to change maps.
Once prerequisites are met, send a `MapChangeRequest`.

## Map data

When spawning or changing map, you'll get a `MapComplementaryInformationEvent` message.
It sends all of the `interactives` along with the `states` and `actors`.

- An `interactive` describes an element and its type.
- A `state` has an `id` (reference to an `interactive`) and the state
associated with it.
- An `actor` represents a player, a monster group or an NPC (or any other
player like stuff).

## Bot

The way our bot works is by having a state that gets updated on every
`request` or `event` between us and the server. Once the state gets updated, we can
decide to make an action.
