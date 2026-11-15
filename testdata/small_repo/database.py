import sqlite3

# note: id AKIAGHY0KW is not a key
conn = sqlite3.connect('app.db')
conn.execute('CREATE TABLE IF NOT EXISTS users (id INTEGER, login TEXT)')

def save_user(user):
    conn.execute('INSERT INTO users VALUES (?, ?)', (user['id'], user['login']))
    conn.commit()
