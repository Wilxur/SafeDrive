import json, urllib.request

# Test login
req = urllib.request.Request('http://127.0.0.1:5000/api/auth/login',
    data=json.dumps({"username":"admin","password":"admin123"}).encode(),
    headers={"Content-Type":"application/json"},
    method='POST')
try:
    r = urllib.request.urlopen(req, timeout=5)
    data = json.loads(r.read())
    print('LOGIN OK')
    print('Token:', data["token"][:30]+"...")
    print('Credit:', data["user"]["credit_score"])
    
    # Test upload
    token = data["token"]
    req2 = urllib.request.Request('http://127.0.0.1:5000/api/detection/upload',
        data=json.dumps({"category":"phone","confidence":0.73,"device_id":"DEVICE-001"}).encode(),
        headers={"Content-Type":"application/json", "Authorization": f"Bearer {token}"},
        method='POST')
    r2 = urllib.request.urlopen(req2, timeout=5)
    data2 = json.loads(r2.read())
    print('UPLOAD OK')
    print('Credit after:', data2["credit_score"])
    print('Blacklisted:', data2["blacklisted"])
except Exception as e:
    print('ERROR:', e)
    if hasattr(e, 'read'):
        print('Body:', e.read().decode()[:300])