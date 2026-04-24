use std::{
    collections::{HashMap, HashSet},
    process::abort,
};

use bevy_math::{CompassQuadrant, Dir2, IVec2};
use rand::{rng, seq::IndexedRandom};

#[unsafe(no_mangle)]
pub extern "C" fn Maze_new(x: i32, y: i32) -> *mut Maze {
    std::panic::catch_unwind(|| {
        let result = Box::new(Maze::new(IVec2 { x, y }));
        Box::into_raw(result)
    })
    .unwrap_or_else(|_| abort())
}

#[unsafe(no_mangle)]
pub extern "C" fn Maze_delete(this: *mut Maze) {
    let _ = unsafe { Box::from_raw(this) };
}

#[unsafe(no_mangle)]
pub extern "C" fn Maze_dims(this: *const Maze) -> IVec2 {
    unsafe { (*this).dims }
}

#[unsafe(no_mangle)]
pub extern "C" fn Maze_has_edge(this: *const Maze, x1: i32, y1: i32, x2: i32, y2: i32) -> i8 {
    let this = unsafe { &*this };
    let a = IVec2 { x: x1, y: y1 };
    let b = IVec2 { x: x2, y: y2 };
    let adjacency = get_adjacency(a, b).unwrap();
    let found = this
        .adjacency
        .get(&a)
        .map(|neighbors| neighbors.contains(&adjacency))
        .unwrap_or(false);
    if found { 1 } else { 0 }
}

pub struct Maze {
    pub dims: IVec2,
    pub adjacency: Adjacency,
}

impl Maze {
    pub fn new(dims: IVec2) -> Self {
        assert!(dims.cmpgt(IVec2::ZERO).all());
        let mut result = Self {
            dims,
            adjacency: Adjacency::new(),
        };
        process(IVec2::ZERO, &mut result);
        result
    }
}

type Adjacency = HashMap<IVec2, HashSet<CompassQuadrant>>;

fn visited(cell: IVec2, adjacency: &Adjacency) -> bool {
    adjacency
        .get(&cell)
        .and_then(|neighbors| (!neighbors.is_empty()).then_some(()))
        .is_some()
}

fn process(cell: IVec2, maze: &mut Maze) {
    if cell == maze.dims - 1 {
        return;
    }
    let unvisited_neighbors = |maze: &Maze| {
        neighbors(cell, maze)
            .into_iter()
            .filter(|x| !visited(*x, &maze.adjacency))
            .collect::<Vec<_>>()
    };
    while let Some(&next) = unvisited_neighbors(maze).choose(&mut rng()) {
        assert!(add_edge(cell, next, &mut maze.adjacency));
        process(next, maze);
    }
}

fn neighbors(cell: IVec2, maze: &Maze) -> HashSet<IVec2> {
    (0..4)
        .map(CompassQuadrant::from_index)
        .map(Option::unwrap)
        .map(Dir2::from)
        .map(|x| cell + x.as_ivec2())
        .filter(|x| x.clamp(IVec2::ZERO, maze.dims - 1) == *x)
        .collect()
}

fn get_adjacency(a: IVec2, b: IVec2) -> Option<CompassQuadrant> {
    match b - a {
        IVec2 { x: 1, y: 0 } => Some(CompassQuadrant::East),
        IVec2 { x: 0, y: 1 } => Some(CompassQuadrant::North),
        IVec2 { x: -1, y: 0 } => Some(CompassQuadrant::West),
        IVec2 { x: 0, y: -1 } => Some(CompassQuadrant::South),
        _ => None,
    }
}

fn add_edge(a: IVec2, b: IVec2, adjacency: &mut Adjacency) -> bool {
    let a_to_b = get_adjacency(a, b).unwrap();
    let first_is_new = adjacency.entry(a).or_default().insert(a_to_b);
    let second_is_new = adjacency.entry(b).or_default().insert(-a_to_b);
    assert_eq!(first_is_new, second_is_new);
    first_is_new
}
