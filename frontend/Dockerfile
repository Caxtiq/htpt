FROM node:18-alpine

WORKDIR /app
COPY package.json .
RUN npm install

COPY . .

# Run NextJS dev server on port 3000
CMD ["npm", "run", "dev"]
