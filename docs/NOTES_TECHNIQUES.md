# Notes techniques et limites de vérification

Ces remarques proviennent de l'inspection du firmware `src/main.cpp`, du modèle Edge Impulse exporté et des trois PDF fournis. Elles ne remplacent pas des essais sur la carte physique.

## 1. Version du projet

- Dans la documentation, le Pico 2 W a été évalué en premier; la version finale du programme fourni est écrite pour l'ESP32-S3.
- Le capteur est l'OV7670, l'écran un TFT ST7735. Les premiers documents mentionnent aussi un OLED I²C : ce n'est plus l'afficheur de la version finale.
- Le modèle Edge Impulse exporté déclare `EI_CLASSIFIER_INPUT_WIDTH = 28`, `HEIGHT = 28`, `LABEL_COUNT = 10`, avec des classes `0` à `9`.
- Le rapport présente un exemple de reconnaissance de chiffre et une grille de validations « Conforme », mais ne donne pas de protocole statistique détaillé ni de nombre de prédictions correctes/total.

## 2. Électricité et batterie

- Le cahier des charges et l'étude de faisabilité décrivaient deux/trois piles AA; le rapport final décrit **quatre piles AA**. Pour le portfolio, expliquer cette évolution.
- Dans `send_image()`, la tension est estimée avec `analogReadMilliVolts(BATT_PIN) * 2 / 1000.0`, puis une valeur 0-100 est calculée sur une plage **3,0 à 4,5 V**. Cette plage ne correspond pas directement à quatre piles AA (environ 6 V nominaux). **Mesurer, calibrer et vérifier le rapport du pont diviseur avant d'affirmer la justesse de l'indicateur de batterie.**
- Ne pas alimenter directement une broche non compatible à partir du pack AA. Mesurer la tension d'entrée, la sortie du convertisseur et les courants transitoires Wi-Fi.

## 3. Réseau et données

- Le programme utilise le port MQTT **1883** (non chiffré) vers `thingsboard.cloud`. Pour un déploiement public, étudier une configuration TLS et la gestion des certificats, suivant les ressources de l'ESP32-S3.
- Le code définit un buffer MQTT à **45 000 octets** et prévoit l'envoi d'un JPEG encodé Base64 toutes les dix secondes. Il faut vérifier la taille réelle du message et la stabilité des publications.
- Un tableau du rapport mentionne un « réseau 5 GHz »; le **Wi-Fi de l'ESP32-S3 est en 2,4 GHz**. Corriger cette mention si le rapport sert de documentation publique.
- Les identifiants Wi-Fi et le jeton ThingsBoard ont été déplacés dans un fichier `secrets.h` local ignoré par Git. **Les anciens identifiants exposés dans les ZIP doivent être remplacés**.

## 4. Reconnaissance de chiffres

- Les paramètres du code définissent : zone centrale `196 x 196`, modèle `28 x 28`, confiance minimale 0,70, trois prédictions identiques pour valider l'affichage.
- `70 %` désigne un seuil de confiance de la classification et ne constitue pas une mesure de performance du système.
- Le programme applique un filtre de contraste et une normalisation; l'éclairage et le cadrage peuvent influencer la reconnaissance.
- Le rapport présente une image du chiffre « 5 » avec une prédiction de 100 % de confiance, mais cela n'établit pas une précision de 100 % sur un jeu d'essai.

## 5. Tests supplémentaires recommandés

1. Mesurer dix à vingt exemples indépendants par chiffre, à plusieurs distances et éclairages. Consigner chiffre attendu, chiffre détecté, score, erreur et temps de réponse.
2. Photographier le montage réel et filmer un test complet : chiffre manuscrit -> caméra -> TFT -> ThingsBoard.
3. Vérifier la présence des quatre trous de fixation du PCB, le positionnement caméra/écran et l'assemblage du boîtier.
4. Tester le projet depuis un **autre ordinateur** avec seulement les fichiers de ce dépôt, et documenter le résultat de compilation.
5. Si vous utilisez GitHub pour vos candidatures, publier un résumé précis de **votre contribution individuelle** au projet collectif.

## 6. Modifications appliquées par l'assistant au paquet préparé

- Remplacement des quatre constantes de connexion codées en dur (SSID, mot de passe, serveur MQTT, jeton) par des noms définis dans `include/secrets.h`.
- Ajout du modèle vide `include/secrets.example.h`.
- Suppression de `build_dir = C:/pb` dans `platformio.ini` afin de rendre le projet portable.
- Exclusion des fichiers de cache `.pio/` et de l'état local `.vscode/`.
- Aucun autre comportement fonctionnel du firmware original n'a été volontairement modifié. **La compilation et le fonctionnement du fichier modifié n'ont pas été testés sur une carte réelle.**
