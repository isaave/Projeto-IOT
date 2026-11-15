#!/bin/zsh
# ============================================================
# Abre o painel web do CloudTech no navegador.
# Basta dar DUPLO CLIQUE neste arquivo.
# ============================================================
PASTA="$(cd "$(dirname "$0")" && pwd)"
cd "$PASTA" || exit 1

PORT=8000
URL="http://localhost:$PORT/index.html"

if curl -s -m 1 -o /dev/null "$URL"; then
  echo "Servidor ja estava rodando na porta $PORT."
else
  echo "Iniciando servidor em $PASTA ..."
  python3 -m http.server "$PORT" &
  SERVIDOR=$!
  sleep 1
fi

open "$URL"

echo ""
echo "Painel: $URL"
echo "Pasta servida: $PASTA"
echo ""
echo "Deixe esta janela aberta. Para encerrar o servidor, pressione Control + C."
echo ""

if [ -n "$SERVIDOR" ]; then
  wait $SERVIDOR
fi
