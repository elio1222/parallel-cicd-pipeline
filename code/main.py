from fastapi import FastAPI
from small_repo.client import fetch_user
from small_repo.database import save_user

app = FastAPI()

@app.get('/users/{user_id}')
def get_user(user_id: int):
    user = fetch_user(user_id)
    save_user(user)
    return user