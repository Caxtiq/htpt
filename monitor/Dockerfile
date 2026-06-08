FROM golang:1.20-alpine

WORKDIR /app
COPY . .

RUN go mod download || true
RUN go build -o monitor main.go

CMD ["./monitor"]
