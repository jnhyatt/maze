build: build-maze

run: build
    ./target/maze

build-maze-gen:
    cd maze-gen && cargo build

build-maze: build-maze-gen
    mkdir -p target
    gcc -std=c23 -o target/maze main.c -L/home/josh/maze/maze-gen/target/debug -lmaze_gen -lGL -lglut -lm

clean:
    cd maze-gen && cargo clean
    rm -rf target
