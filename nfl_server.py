from flask import Flask, jsonify
import requests
import logging

app = Flask(__name__)

# Configure logging
logging.basicConfig(level=logging.INFO)

@app.route('/nfl/scores', methods=['GET'])
def get_nfl_scores():
    """
    Fetch NFL scores from ESPN and return simplified JSON
    """
    espn_url = "http://site.api.espn.com/apis/site/v2/sports/football/nfl/scoreboard"
    
    try:
        # Fetch from ESPN
        response = requests.get(espn_url, timeout=10)
        
        if response.status_code != 200:
            return jsonify({"error": "Failed to fetch from ESPN"}), 500
        
        data = response.json()
        
        # Simplify the data for ESP32
        games = []
        
        if 'events' in data:
            for event in data['events']:
                try:
                    competition = event['competitions'][0]
                    competitors = competition['competitors']
                    status = competition['status']
                    
                    # Find home and away teams
                    home_team = next(c for c in competitors if c['homeAway'] == 'home')
                    away_team = next(c for c in competitors if c['homeAway'] == 'away')
                    
                    # Get status information
                    state = status['type']['state']
                    status_name = status['type']['name']
                    
                    # Determine if game is live, final, or upcoming
                    is_live = state == 'in'
                    is_final = state == 'post' or status_name == 'STATUS_FINAL'
                    is_upcoming = state == 'pre' or status_name == 'STATUS_SCHEDULED'
                    
                    # Get possession information
                    possession = ""
                    if 'situation' in competition:
                        situation = competition['situation']
                        if 'possession' in situation:
                            possession_id = situation['possession']
                            # Match possession ID to team
                            if possession_id == home_team['id']:
                                possession = "home"
                            elif possession_id == away_team['id']:
                                possession = "away"
                    
                    game = {
                        'away': {
                            'abbr': away_team['team']['abbreviation'],
                            'score': away_team.get('score', 0)
                        },
                        'home': {
                            'abbr': home_team['team']['abbreviation'],
                            'score': home_team.get('score', 0)
                        },
                        'status': {
                            'state': state,
                            'detail': status['type']['shortDetail']
                        },
                        'live': is_live,
                        'final': is_final,
                        'upcoming': is_upcoming,
                        'possession': possession  # ADD THIS LINE
                    }
                    
                    games.append(game)
                    
                except (KeyError, IndexError) as e:
                    logging.warning(f"Error parsing game: {e}")
                    continue
        
        return jsonify({
            'count': len(games),
            'games': games
        })
    
    except requests.exceptions.RequestException as e:
        logging.error(f"Request error: {e}")
        return jsonify({"error": str(e)}), 500
    except Exception as e:
        logging.error(f"Unexpected error: {e}")
        return jsonify({"error": "Internal server error"}), 500

@app.route('/health', methods=['GET'])
def health_check():
    """Simple health check endpoint"""
    return jsonify({"status": "ok"})

if __name__ == '__main__':
    # Run on all interfaces so ESP32 can access it
    app.run(host='0.0.0.0', port=5001, debug=False)
