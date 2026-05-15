#!/bin/bash

# Script to create a signing keystore for release builds

echo "=== Creating Android Signing Keystore ==="
echo ""

KEYSTORE_DIR="$(dirname "$0")/../android/app"
KEYSTORE_FILE="$KEYSTORE_DIR/monerominer.keystore"

if [ -f "$KEYSTORE_FILE" ]; then
    echo "⚠️  Keystore already exists at: $KEYSTORE_FILE"
    read -p "Do you want to overwrite it? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 0
    fi
    rm "$KEYSTORE_FILE"
fi

echo "Please enter the following information:"
echo ""

read -p "Alias (default: monerominer): " ALIAS
ALIAS=${ALIAS:-monerominer}

read -sp "Keystore password (min 6 characters): " STORE_PASS
echo ""

read -sp "Key password (or Enter to use keystore password): " KEY_PASS
echo ""

if [ -z "$KEY_PASS" ]; then
    KEY_PASS="$STORE_PASS"
fi

read -p "First and Last Name: " NAME
read -p "Organizational Unit: " ORG_UNIT
read -p "Organization: " ORG
read -p "City/Locality: " CITY
read -p "State/Province: " STATE
read -p "Country Code (2 letters): " COUNTRY

if [ ${#STORE_PASS} -lt 6 ]; then
    echo "❌ Password must be at least 6 characters"
    exit 1
fi

# Generate keystore
keytool -genkey -v \
    -keystore "$KEYSTORE_FILE" \
    -alias "$ALIAS" \
    -keyalg RSA \
    -keysize 2048 \
    -validity 10000 \
    -storepass "$STORE_PASS" \
    -keypass "$KEY_PASS" \
    -dname "CN=$NAME, OU=$ORG_UNIT, O=$ORG, L=$CITY, S=$STATE, C=$COUNTRY"

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Keystore created successfully!"
    echo "Location: $KEYSTORE_FILE"
    echo ""
    echo "Add these to your ~/.gradle/gradle.properties or set as environment variables:"
    echo ""
    echo "MONEROMINER_STORE_FILE=app/monerominer.keystore"
    echo "MONEROMINER_STORE_PASSWORD=$STORE_PASS"
    echo "MONEROMINER_KEY_ALIAS=$ALIAS"
    echo "MONEROMINER_KEY_PASSWORD=$KEY_PASS"
    echo ""
    echo "⚠️  Keep these credentials secure and never commit them to version control!"
else
    echo "❌ Failed to create keystore"
    exit 1
fi
