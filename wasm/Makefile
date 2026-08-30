EMXX ?= em++

all: crispydrums.js

crispydrums.js: CrispyDrumsWasm.cpp
	$(EMXX) CrispyDrumsWasm.cpp -O3 -std=c++17 \
		-s WASM=1 \
		-s ALLOW_MEMORY_GROWTH=1 \
		-s EXPORTED_FUNCTIONS='["_crispy_render_hit","_malloc","_free"]' \
		-s EXPORTED_RUNTIME_METHODS='["cwrap"]' \
		-s ENVIRONMENT=web \
		-o crispydrums.js

serve: all
	python3 -m http.server 8000

clean:
	rm -f crispydrums.js crispydrums.wasm

.PHONY: all serve clean
