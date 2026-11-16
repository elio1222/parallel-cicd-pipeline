import requests

GITHUB_TOKEN = "ghp_ajhS0TKBeqaVVppyc0aD4Mj31H1aX5MrqdyQ"

def fetch_user(user_id):
    r = requests.get(f'https://api.github.com/user/{user_id}',
                     headers={'Authorization': f'token {GITHUB_TOKEN}'})
    return r.json()
