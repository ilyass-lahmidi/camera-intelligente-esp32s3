# Publier ce projet sur GitHub — procédure Windows

## Avant de commencer

- Ne téléversez **pas** les ZIP originaux : ils contiennent d'anciens identifiants.
- Le dossier à utiliser est celui qui contient `README.md`, `platformio.ini`, `src/` et `lib/`.
- Vérifiez `docs/AVANT_PUBLICATION.md` et, tant que les autorisations ne sont pas confirmées, créez un dépôt **privé**.

## Étape 1 : créer un compte GitHub

1. Ouvrir https://github.com/signup et suivre les instructions.
2. Choisir un nom d'utilisateur professionnel, que vous conserverez pour vos candidatures.
3. Vérifier votre adresse courriel et activer l'authentification à deux facteurs lorsque proposé.

## Étape 2 : installer GitHub Desktop

1. Aller sur https://desktop.github.com/ et installer GitHub Desktop.
2. Se connecter avec son compte GitHub.
3. Décompresser l'archive préparée dans `Documents/GitHub/camera-intelligente-esp32s3/`.
4. Dans GitHub Desktop, sélectionner **File > Add local repository...** puis choisir ce dossier.
5. Si GitHub Desktop indique que ce dossier n'est pas un dépôt Git, choisir l'option pour **créer un dépôt ici**. (Les intitulés peuvent varier selon la version.)

## Étape 3 : vérifier le premier commit

1. Ouvrir l'onglet **Changes** dans GitHub Desktop.
2. Vérifier que `include/secrets.h`, les dossiers `.pio/` et les anciens ZIP n'apparaissent pas.
3. Vérifier les fichiers `README.md`, `src/main.cpp`, `platformio.ini`, `lib/`, `hardware/`, `mechanical/` et `images/`.
4. Dans **Summary**, écrire `Ajout initial du projet caméra intelligente`.
5. Cliquer sur **Commit to main**.

## Étape 4 : publier le dépôt, d'abord privé

1. Cliquer sur **Publish repository**.
2. Nom : `camera-intelligente-esp32s3`.
3. Description proposée : `Reconnaissance de chiffres manuscrits sur ESP32-S3 avec OV7670, Edge Impulse, TFT, MQTT et conception PCB/3D.`
4. Laisser **Keep this code private** coché en attendant l'accord de l'équipe et la vérification de sécurité.
5. Cliquer sur **Publish Repository**.
6. Ouvrir le dépôt en ligne et vérifier l'affichage automatique du `README.md`.

## Étape 5 : vérifier les crédits et passer en public ultérieurement

- Identifier honnêtement les contributions de chacun.
- Confirmer les droits de publication des conceptions et des images.
- Vérifier que la version sécurisée est celle qui est publiée.
- Une fois prêt, adapter la visibilité du dépôt depuis ses paramètres (Settings), en vérifiant l'option disponible pour votre compte.

## Étape 6 : utiliser le dépôt dans un CV

- Ouvrir le dépôt sur le navigateur et copier son adresse URL.
- Insérer un lien cliquable dans le PDF du CV, idéalement en en-tête et/ou à la description du projet.
- Épingler le dépôt dans la section de personnalisation du profil GitHub.
- Ajouter plus tard une vraie photo et une vidéo de démonstration (pas seulement les captures de conception).

## Mettre à jour le code

Après une modification locale, revenir dans GitHub Desktop : **Changes > Summary > Commit to main > Push origin**. Écrire des messages de commit décrivant les changements, par exemple `Amélioration du filtrage des prédictions` ou `Documentation des tests de batterie`.

**Important :** la première publication de ce dossier n'invente pas l'historique réel de développement qui ne figure pas dans les ZIP fournis.
