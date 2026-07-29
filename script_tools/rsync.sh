#!/bin/bash
set -euo pipefail

REMOTE_HOST="pi@rpi2w.local"
REMOTE_DIR="/home/pi/src"
PROJECT_NAME="gpio_generator_16bits"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SECRET_FILE="$SCRIPT_DIR/.sshpass.env"
LOCAL_BASE="$(dirname "$SCRIPT_DIR")"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

info()    { echo -e "${BLUE}[INFO]${NC} $*"; }
success() { echo -e "${GREEN}[OK]${NC} $*"; }
warn()    { echo -e "${YELLOW}[WARN]${NC} $*"; }
error()   { echo -e "${RED}[ERROR]${NC} $*"; }

load_sshpass() {
    if [[ -n "${SSHPASS:-}" ]]; then
        return 0
    fi
    if [[ -f "$SECRET_FILE" ]]; then
        set -a
        # shellcheck disable=SC1090
        source "$SECRET_FILE"
        set +a
        return 0
    fi
    return 1
}

#ingresar contraseña , aqui debe preguntar
prompt_password() {
    echo
    echo "Se requiere la contraseña SSH para $REMOTE_HOST"
    echo
    echo "Opciones:"
    echo "  1) Ingresar contraseña y guardarla localmente (recomendado)"
    echo "  2) Ingresar contraseña solo para esta sesión"
    echo "  3) Usar variable de entorno SSHPASS ya exportada"
    echo "  4) Salir"
    echo
    read -rp "Selecciona una opción [1-4]: " opt
    case "$opt" in
        1)
            read -rsp "Contraseña SSH: " pass
            echo
            cat > "$SECRET_FILE" <<EOF
SSHPASS='$pass'
EOF
            chmod 600 "$SECRET_FILE"
            # shellcheck disable=SC1090
            source "$SECRET_FILE"
            success "Contraseña guardada en: $SECRET_FILE"
            ;;
        2)
            read -rsp "Contraseña SSH: " pass
            echo
            export SSHPASS="$pass"
            export SSHPASS
            success "Variable SSHPASS exportada para esta sesión."
            ;;
        3)
            if [[ -z "${SSHPASS:-}" ]]; then
                error "SSHPASS no está exportada en el entorno actual."
                exit 1
            fi
            success "Usando SSHPASS del entorno."
            ;;
        *)
            error "Operación cancelada."
            exit 1
            ;;
    esac
}

check_sshpass() {
    if ! load_sshpass; then
        prompt_password
    fi
    [[ -z "${SSHPASS:-}" ]] && { error "SSHPASS no configurado."; exit 1; }
}

banner() {
    echo
    echo -e "${CYAN}========================================"
    echo "  RSYNC - gpio_generator_16bits"
    echo -e "========================================${NC}"
    echo
}

menu_main() {
    banner
    echo "Host remoto: $REMOTE_HOST"
    echo "Directorio remoto base: $REMOTE_DIR"
    echo
    echo "1) Transferir proyecto completo"
    echo "2) Transferir carpeta específica"
    echo "3) Transferir archivo(s) específico(s)"
    echo "4) Configurar contraseña SSH (SSHPASS)"
    echo "5) Ver ejemplos de uso"
    echo "0) Salir"
    echo
    read -rp "Selecciona una opción [0-5]: " opt
    echo
    case "$opt" in
        1) menu_transfer_project ;;
        2) menu_transfer_folder ;;
        3) menu_transfer_files ;;
        4) prompt_password ;;
        5) show_examples ;;
        0) exit 0 ;;
        *) error "Opción inválida"; menu_main ;;
    esac
}

transfer() {
    local src="$1"
    local dst="$2"
    local label="$3"

    info "Transferiendo: $label"
    echo "  Origen: $src"
    echo "  Destino: $dst"
    echo

    if rsync -avzP \
        --rsh="sshpass -e ssh" \
        --exclude='.git' \
        --exclude='obj/' \
        --exclude='bin/' \
        --exclude='logs/' \
        --exclude='*.log' \
        "$src" "$dst"; then
        success "Transferencia completada: $label"
    else
        error "Falló la transferencia: $label"
        return 1
    fi
}

menu_transfer_project() {
    check_sshpass

    local src="$LOCAL_BASE/$PROJECT_NAME/"
    local dst="$REMOTE_HOST:$REMOTE_DIR/$PROJECT_NAME/"

    transfer "$src" "$dst" "Proyecto completo ($PROJECT_NAME/)"

    read -rp "¿Deseas compilar en la Raspberry Pi ahora? [s/N]: " compile_opt
    if [[ "$compile_opt" =~ ^[sSyY]$ ]]; then
        remote_compile
    fi

    menu_main
}

menu_transfer_folder() {
    check_sshpass

    echo "Carpetas locales disponibles en $LOCAL_BASE/$PROJECT_NAME/:"
    echo
    local -a dirs=()
    local i=1
    while IFS= read -r d; do
        local rel="${d#$LOCAL_BASE/$PROJECT_NAME/}"
        [[ -z "$rel" ]] && continue
        echo "  $i) $rel"
        dirs+=("$rel")
        ((i++))
    done < <(find "$LOCAL_BASE/$PROJECT_NAME" -mindepth 1 -maxdepth 2 -type d | sort)

    if [[ ${#dirs[@]} -eq 0 ]]; then
        warn "No hay carpetas para transferir."
        menu_main
        return
    fi

    echo
    read -rp "Selecciona una carpeta por número (0 para cancelar): " sel
    if [[ "$sel" == "0" ]] || ! [[ "$sel" =~ ^[0-9]+$ ]] || (( sel < 1 || sel > ${#dirs[@]} )); then
        warn "Selección cancelada o inválida."
        menu_main
        return
    fi

    local selected="${dirs[$((sel - 1))]}"
    local src="$LOCAL_BASE/$PROJECT_NAME/$selected/"
    local dst="$REMOTE_HOST:$REMOTE_DIR/$PROJECT_NAME/$selected/"

    transfer "$src" "$dst" "Carpeta: $selected"

    menu_main
}

menu_transfer_files() {
    check_sshpass

    echo "Archivos/carpetas locales en $LOCAL_BASE/$PROJECT_NAME/:"
    echo
    local -a paths=()
    local i=1
    while IFS= read -r p; do
        local rel="${p#$LOCAL_BASE/$PROJECT_NAME/}"
        [[ -z "$rel" ]] && continue
        echo "  $i) $rel"
        paths+=("$rel")
        ((i++))
    done < <(find "$LOCAL_BASE/$PROJECT_NAME" -mindepth 1 -maxdepth 2 | sort)

    if [[ ${#paths[@]} -eq 0 ]]; then
        warn "No hay archivos/carpetas para transferir."
        menu_main
        return
    fi

    echo
    read -rp "Selecciona uno o más elementos por número, separados por espacio (0 para cancelar): " -a sel
    for s in "${sel[@]}"; do
        if [[ "$s" == "0" ]] || ! [[ "$s" =~ ^[0-9]+$ ]] || (( s < 1 || s > ${#paths[@]} )); then
            warn "Selección cancelada o inválida."
            menu_main
            return
        fi
    done

    for s in "${sel[@]}"; do
        local selected="${paths[$((s - 1))]}"
        local src="$LOCAL_BASE/$PROJECT_NAME/$selected"
        local dst="$REMOTE_HOST:$REMOTE_DIR/$PROJECT_NAME/$selected"

        if [[ -d "$src" ]]; then
            src="$src/"
        fi

        transfer "$src" "$dst" "Elemento: $selected"
    done

    menu_main
}

remote_compile() {
    info "Compilando proyecto en la Raspberry Pi..."
    if sshpass -e ssh "$REMOTE_HOST" "cd $REMOTE_DIR/$PROJECT_NAME/gpios && make clean && make"; then
        success "Compilación exitosa en la Raspberry Pi."
    else
        error "Falló la compilación en la Raspberry Pi."
    fi
}

show_examples() {
    clear
    echo -e "${CYAN}========================================"
    echo "  EJEMPLOS DE USO"
    echo -e "========================================${NC}"
    echo
    cat <<'EOF'
1) Transferir solo el proyecto completo:
   ./script_tools/rsync.sh
   -> Menú opción 1

2) Transferir solo la carpeta gpios/:
   ./script_tools/rsync.sh
   -> Menú opción 2
   -> Seleccionar gpios

3) Transferir un archivo específico:
   ./script_tools/rsync.sh
   -> Menú opción 3
   -> Seleccionar src/main.cpp

4) Compilar automáticamente después de transferir:
   ./script_tools/rsync.sh
   -> Menú opción 1
   -> Responder 's' cuando pregunte

5) Transferencia manual desde terminal:
   export SSHPASS='tu-contraseña'
   rsync -avzP --rsh="sshpass -e ssh" gpio_generator_16bits/ pi@rpi2w.local:/home/pi/src/

6) Transferencia manual solo carpeta gpios/:
   export SSHPASS='tu-contraseña'
   rsync -avzP --rsh="sshpass -e ssh" gpio_generator_16bits/gpios/ pi@rpi2w.local:/home/pi/src/gpio_generator_16bits/gpios/

7) Compilar en la Raspberry Pi:
   ssh pi@rpi2w.local
   cd /home/pi/src/gpio_generator_16bits/gpios
   make clean && make

8) Ejecutar en la Raspberry Pi (120 segundos por defecto):
   cd /home/pi/src/gpio_generator_16bits/gpios
   sudo ./bin/gpio_generator 120

9) Ejecutar por tiempo personalizado (ej: 60 segundos):
   sudo ./bin/gpio_generator 60
EOF

    echo
    read -rp "Presiona ENTER para volver al menú principal..."
    menu_main
}

# Inicio
[[ "${BASH_SOURCE[0]}" == "$0" ]] && menu_main
