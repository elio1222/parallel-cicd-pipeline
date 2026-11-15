from fastapi import FastAPI
from client import fetch_user
from database import save_user

app = FastAPI()

@app.get('/users/{user_id}')
def get_user(user_id: int):
    user = fetch_user(user_id)
    save_user(user)
    return user