# Makefile principal - Build all epaper projects
# Usage: make clean  (limpia todos los proyectos)
#        make        (compila todos los proyectos)
#        make -j4    (compila en paralelo)

PROJECTS = gpios

.PHONY: all clean $(PROJECTS)

all: $(PROJECTS)

$(PROJECTS):
	@echo "========================================"
	@echo "Building: $@"
	@echo "========================================"
	$(MAKE) -C $@
	@echo ""

clean:
	@echo "========================================"
	@echo "Cleaning all projects..."
	@echo "========================================"
	@for proj in $(PROJECTS); do \
		echo "Cleaning: $$proj"; \
		$(MAKE) -C $$proj clean; \
	done
	@echo "All projects cleaned."

.PHONY: help
help:
	@echo "Targets:"
	@echo "  all     - Build all projects (default)"
	@echo "  clean   - Clean all projects"
	@echo "  help    - Show this help"
	@echo ""
	@echo "Projects: $(PROJECTS)"
