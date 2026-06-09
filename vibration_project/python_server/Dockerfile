FROM python:3.11-slim

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY . .

ENV PORT=7860
ENV SOCKETIO_ASYNC_MODE=eventlet

EXPOSE 7860

CMD sh -c "gunicorn --worker-class eventlet -w 1 --bind 0.0.0.0:${PORT} --timeout 120 app:app"
