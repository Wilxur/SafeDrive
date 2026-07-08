import os
from app import create_app

app = create_app()

if __name__ == '__main__':
    env = os.getenv('FLASK_ENV', 'development')
    if env == 'production':
        app.run(host='0.0.0.0', port=5000, debug=False)
    else:
        debug = os.getenv('FLASK_DEBUG', '1') != '0'
        app.run(host='0.0.0.0', port=5000, debug=debug)
